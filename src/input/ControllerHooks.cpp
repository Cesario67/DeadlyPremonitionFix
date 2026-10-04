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
};

std::array<Controller, kMaxJoysticks> g_controllers{};
std::atomic<int> g_diagnosticLines{0};

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

MMRESULT WINAPI HookJoyGetPosEx(UINT id, LPJOYINFOEX info) {
    const MMRESULT result = g_joyGetPosEx(id, info);
    if (id >= kMaxJoysticks) {
        return result;
    }
    Controller& controller = g_controllers[id];
    if (result != JOYERR_NOERROR || info == nullptr) {
        if (controller.connected) {
            controller.connected = false;
            controller.identified = false;  // réidentifier au rebranchement (autre manette possible)
            log::Info("Manette {} déconnectée", id);
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
    if (!config.sonyControllerLayout && !config.controllerDiagnostics) {
        log::Info("Manettes : conversion et diagnostic désactivés");
        return;
    }
    // winmm.dll est importée par DP.exe : déjà chargée, GetModuleHandle suffit (pas de LoadLibrary dans DllMain).
    if (const HMODULE winmm = GetModuleHandleW(L"winmm.dll")) {
        g_joyGetDevCapsW = reinterpret_cast<JoyGetDevCapsWFn>(GetProcAddress(winmm, "joyGetDevCapsW"));
    }
    if (!hooking::PatchImport(gameModule, "winmm.dll", "joyGetPosEx", &HookJoyGetPosEx, &g_joyGetPosEx)) {
        log::Warn("Interception de joyGetPosEx impossible : manettes non converties");
        return;
    }
    log::Info("Manettes : disposition Xbox pour les manettes Sony : {} | diagnostic : {} | gâchettes inversées : {}",
              config.sonyControllerLayout ? "oui" : "non", config.controllerDiagnostics ? "oui" : "non",
              config.swapControllerTriggers ? "oui" : "non");
}

}  // namespace dpsf::input
