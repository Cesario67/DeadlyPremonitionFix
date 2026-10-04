#include "input/ControllerHooks.h"

#include <mmsystem.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <string>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Strings.h"
#include "input/ControllerMapping.h"

namespace dpsf::input {

namespace {

using JoyGetPosExFn = MMRESULT(WINAPI*)(UINT, LPJOYINFOEX);
using JoyGetDevCapsWFn = MMRESULT(WINAPI*)(UINT_PTR, LPJOYCAPSW, UINT);

constexpr UINT kMaxJoysticks = 16;
constexpr ULONGLONG kDiagnosticIntervalMs = 250;
constexpr int kMaxDiagnosticLines = 3000;
constexpr DWORD kAxisChangeThreshold = 6000;  // ~10 % de la course : ignore le bruit des sticks
constexpr double kSlowCallMs = 10.0;
constexpr int kMaxSlowCallLines = 500;

JoyGetPosExFn g_joyGetPosEx = nullptr;
JoyGetDevCapsWFn g_joyGetDevCapsW = nullptr;

// État par manette, mis à jour par le thread qui interroge les manettes (un seul dans DP.exe).
struct Controller {
    bool identified = false;
    bool connected = false;
    ControllerFamily family = ControllerFamily::Other;
    AxisRanges ranges{};
    JOYINFOEX lastLogged{};
    ULONGLONG lastLogTick = 0;
    MMRESULT lastError = JOYERR_NOERROR;  // dernière erreur journalisée (une ligne par code)
};

std::array<Controller, kMaxJoysticks> g_controllers{};
std::atomic<int> g_diagnosticLines{0};

// Numéros de manette sans manette : réponse de WinMM mémorisée (JOYERR_NOERROR = rien en mémoire), rendue
// au jeu sans appeler WinMM. Voir Config::cacheAbsentControllers.
constexpr DWORD kAbsentRecheckMs = 3000;
bool g_cacheAbsent = false;
std::array<std::atomic<MMRESULT>, kMaxJoysticks> g_absentResult{};
std::atomic<bool> g_watcherStarted{false};

// Vérifie en arrière-plan si une manette a été branchée sur un numéro mémorisé comme vide : la réénumération
// lente de WinMM a lieu sur ce thread, plus sur celui du jeu.
DWORD WINAPI AbsentControllerWatcher(LPVOID) {
    for (;;) {
        Sleep(kAbsentRecheckMs);
        for (UINT id = 0; id < kMaxJoysticks; ++id) {
            if (g_absentResult[id].load() == JOYERR_NOERROR) {
                continue;
            }
            JOYINFOEX probe{};
            probe.dwSize = sizeof(probe);
            probe.dwFlags = JOY_RETURNALL;
            if (g_joyGetPosEx(id, &probe) == JOYERR_NOERROR) {
                g_absentResult[id].store(JOYERR_NOERROR);
                log::Info("Manette {} branchée : de nouveau lue par le jeu", id);
            }
        }
    }
}

void RememberAbsent(UINT id, MMRESULT result) {
    if (g_absentResult[id].exchange(result) == JOYERR_NOERROR) {
        log::Info("Manette {} absente (code {}) : le mod répond à la place de WinMM", id, result);
    }
    if (!g_watcherStarted.exchange(true)) {
        if (const HANDLE thread = CreateThread(nullptr, 0, &AbsentControllerWatcher, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        } else {
            log::Warn("Thread de détection des manettes impossible à créer (erreur {})", GetLastError());
        }
    }
}

const char* FamilyName(ControllerFamily family) noexcept {
    return family == ControllerFamily::Sony ? "Sony (DualShock 4 / DualSense)" : "autre";
}

void Identify(UINT id, Controller& controller) {
    controller.identified = true;
    JOYCAPSW caps{};
    if (g_joyGetDevCapsW == nullptr || g_joyGetDevCapsW(id, &caps, sizeof(caps)) != JOYERR_NOERROR) {
        log::Warn("Manette {} : caractéristiques illisibles (joyGetDevCaps), conversion désactivée", id);
        return;
    }
    controller.family = IdentifyController(caps);
    controller.ranges = RangesFromCaps(caps);
    const Config& config = GetConfig();
    const bool remap = controller.family == ControllerFamily::Sony && config.sonyControllerLayout;
    log::Info("Manette {} : « {} », fabricant 0x{:04X}, produit 0x{:04X}, {} axes, {} boutons, famille {} | "
              "disposition Xbox : {}",
              id, WideToUtf8(caps.szPname), caps.wMid, caps.wPid, caps.wNumAxes, caps.wNumButtons,
              FamilyName(controller.family), remap ? "oui" : "non");
    log::Info("Manette {} : plages X {}-{} Y {}-{} Z {}-{} R {}-{} U {}-{} V {}-{}", id, caps.wXmin, caps.wXmax,
              caps.wYmin, caps.wYmax, caps.wZmin, caps.wZmax, caps.wRmin, caps.wRmax, caps.wUmin, caps.wUmax,
              caps.wVmin, caps.wVmax);
}

bool AxisMoved(DWORD a, DWORD b) noexcept {
    return (a > b ? a - b : b - a) > kAxisChangeThreshold;
}

bool StateChanged(const JOYINFOEX& a, const JOYINFOEX& b) noexcept {
    return AxisMoved(a.dwXpos, b.dwXpos) || AxisMoved(a.dwYpos, b.dwYpos) || AxisMoved(a.dwZpos, b.dwZpos) ||
           AxisMoved(a.dwRpos, b.dwRpos) || AxisMoved(a.dwUpos, b.dwUpos) || AxisMoved(a.dwVpos, b.dwVpos) ||
           a.dwButtons != b.dwButtons || a.dwPOV != b.dwPOV;
}

// Journal de diagnostic : état brut (et converti) quand il change, au plus 4 lignes par seconde par manette.
// Sert à vérifier la correspondance des axes : au repos, puis en bougeant un stick ou une gâchette à la fois.
void LogDiagnostic(UINT id, Controller& controller, const JOYINFOEX& raw, const JOYINFOEX* converted, bool first) {
    const ULONGLONG now = GetTickCount64();
    if (!first && (!StateChanged(raw, controller.lastLogged) || now - controller.lastLogTick < kDiagnosticIntervalMs)) {
        return;
    }
    if (g_diagnosticLines.fetch_add(1) >= kMaxDiagnosticLines) {
        return;
    }
    controller.lastLogged = raw;
    controller.lastLogTick = now;
    std::string line = std::format("Manette {}{} brut : X={} Y={} Z={} R={} U={} V={} boutons=0x{:04X} POV={}", id,
                                   first ? " (au repos ?)" : "", raw.dwXpos, raw.dwYpos, raw.dwZpos, raw.dwRpos,
                                   raw.dwUpos, raw.dwVpos, raw.dwButtons, raw.dwPOV);
    if (converted != nullptr) {
        line += std::format(" | vu par le jeu : X={} Y={} Z={} R={} U={} boutons=0x{:04X}", converted->dwXpos,
                            converted->dwYpos, converted->dwZpos, converted->dwRpos, converted->dwUpos,
                            converted->dwButtons);
    }
    log::Write(log::Level::Info, line);
    if (g_diagnosticLines.load() == kMaxDiagnosticLines) {
        log::Info("Diagnostic manette : limite de {} lignes atteinte, journalisation arrêtée", kMaxDiagnosticLines);
    }
}

// Piste à vérifier pour la saccade régulière (~65 ms toutes les 20 s, 04/10/2026) : WinMM peut réénumérer
// les manettes pendant un appel. Les appels lents sont journalisés pour le confirmer ou l'écarter.
void LogSlowCall(UINT id, MMRESULT result, LONGLONG start, LONGLONG end) {
    static std::atomic<int> s_lines{0};
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    const double ms = static_cast<double>(end - start) * 1000.0 / static_cast<double>(frequency.QuadPart);
    if (ms >= kSlowCallMs && s_lines.fetch_add(1) < kMaxSlowCallLines) {
        log::Warn("Manette {} : joyGetPosEx a pris {:.1f} ms (code {})", id, ms, result);
    }
}

// Diagnostic du 04/10/2026 : en jeu, la DualSense renvoie par moments des valeurs fixes (127, 32767) et se
// « débranche » plusieurs fois par seconde avec certaines versions du mod. Journalise chaque nouvelle façon
// dont le jeu demande la lecture (taille de la structure, drapeaux JOY_RETURN*), qui détermine ce que
// WinMM renvoie (JOY_RETURNRAWDATA : valeurs brutes, par exemple).
void LogRequest(UINT id, DWORD size, DWORD flags) {
    constexpr size_t kMaxKinds = 8;
    struct Kind {
        DWORD size;
        DWORD flags;
    };
    static std::array<std::array<Kind, kMaxKinds>, kMaxJoysticks> s_kinds{};
    static std::array<size_t, kMaxJoysticks> s_counts{};
    std::array<Kind, kMaxKinds>& kinds = s_kinds[id];
    size_t& count = s_counts[id];
    for (size_t i = 0; i < count; ++i) {
        if (kinds[i].size == size && kinds[i].flags == flags) {
            return;
        }
    }
    if (count < kMaxKinds) {
        kinds[count++] = {size, flags};
        log::Info("Manette {} : lecture demandée par le jeu, taille {}, drapeaux 0x{:X}", id, size, flags);
    }
}

MMRESULT WINAPI HookJoyGetPosEx(UINT id, LPJOYINFOEX info) {
    // Seuls les appels bien formés sont mis en cache : DP.exe fait aussi un appel avec dwSize = 6 qui échoue
    // même sur une manette branchée.
    const bool wellFormed = info != nullptr && info->dwSize == sizeof(JOYINFOEX);
    if (g_cacheAbsent && wellFormed && id < kMaxJoysticks) {
        const MMRESULT cached = g_absentResult[id].load(std::memory_order_relaxed);
        if (cached != JOYERR_NOERROR) {
            return cached;
        }
    }
    if (info != nullptr && id < kMaxJoysticks) {
        LogRequest(id, info->dwSize, info->dwFlags);
    }
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    QueryPerformanceCounter(&start);
    const MMRESULT result = g_joyGetPosEx(id, info);
    QueryPerformanceCounter(&end);
    LogSlowCall(id, result, start.QuadPart, end.QuadPart);
    if (id >= kMaxJoysticks) {
        return result;
    }
    Controller& controller = g_controllers[id];
    if (result != JOYERR_NOERROR || info == nullptr) {
        // DP.exe interroge la même manette depuis deux endroits et l'un des appels échoue à chaque image
        // (journal du 04/10/2026) : seul JOYERR_UNPLUGGED signifie une manette débranchée. Sinon la manette
        // était réidentifiée (joyGetDevCaps) et journalisée à chaque image.
        if (result == JOYERR_UNPLUGGED) {
            if (controller.connected) {
                controller.connected = false;
                controller.identified = false;  // réidentifier au rebranchement (autre manette possible)
                log::Info("Manette {} déconnectée", id);
            }
        } else if (result != controller.lastError) {
            controller.lastError = result;
            log::Info("Manette {} : joyGetPosEx refusé par Windows (code {}, drapeaux 0x{:X}, taille {})", id, result,
                      info != nullptr ? info->dwFlags : 0, info != nullptr ? info->dwSize : 0);
        }
        if (g_cacheAbsent && wellFormed && (result == JOYERR_PARMS || result == JOYERR_UNPLUGGED)) {
            RememberAbsent(id, result);
        }
        return result;
    }
    try {
        const bool first = !controller.connected;
        controller.connected = true;
        if (!controller.identified) {
            Identify(id, controller);
        }
        const Config& config = GetConfig();
        const JOYINFOEX raw = *info;
        const bool remap = controller.family == ControllerFamily::Sony && config.sonyControllerLayout;
        if (remap) {
            SonyToXboxLayout(*info, controller.ranges, config.swapControllerTriggers);
        }
        if (config.controllerDiagnostics) {
            LogDiagnostic(id, controller, raw, remap ? info : nullptr, first);
        }
    } catch (...) {
        // Jamais d'exception vers le jeu : l'état brut lui est rendu tel quel.
    }
    return result;
}

}  // namespace

void Install(HMODULE gameModule) {
    const Config& config = GetConfig();
    if (!config.sonyControllerLayout && !config.controllerDiagnostics && !config.cacheAbsentControllers) {
        log::Info("Manettes : conversion, diagnostic et cache des manettes absentes désactivés");
        return;
    }
    g_cacheAbsent = config.cacheAbsentControllers;
    // winmm.dll est importée par DP.exe : déjà chargée, GetModuleHandle suffit (pas de LoadLibrary dans DllMain).
    if (const HMODULE winmm = GetModuleHandleW(L"winmm.dll")) {
        g_joyGetDevCapsW = reinterpret_cast<JoyGetDevCapsWFn>(GetProcAddress(winmm, "joyGetDevCapsW"));
    }
    if (!hooking::PatchImport(gameModule, "winmm.dll", "joyGetPosEx", &HookJoyGetPosEx, &g_joyGetPosEx)) {
        log::Warn("Interception de joyGetPosEx impossible : manettes non converties");
        return;
    }
    log::Info("Manettes : disposition Xbox pour les manettes Sony : {} | diagnostic : {} | gâchettes inversées : {} | "
              "cache des manettes absentes : {}",
              config.sonyControllerLayout ? "oui" : "non", config.controllerDiagnostics ? "oui" : "non",
              config.swapControllerTriggers ? "oui" : "non", config.cacheAbsentControllers ? "oui" : "non");
}

}  // namespace dpsf::input
