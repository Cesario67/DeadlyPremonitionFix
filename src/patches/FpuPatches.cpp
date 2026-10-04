#include "patches/FpuPatches.h"

#include <MinHook.h>
#include <float.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <span>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SystemInfo.h"

namespace dpsf::patches {

namespace {

// Version analysée : DP.exe 1.01b Steam (horodatage PE 0x529721DC).
constexpr DWORD kSteam101bTimestamp = 0x529721DC;

using MicrosecondsFn = long long(__cdecl*)();
using SecondsFn = double(__cdecl*)(int);
using AimHandlerFn = void(__cdecl*)();

MicrosecondsFn g_microseconds = nullptr;
SecondsFn g_seconds = nullptr;
AimHandlerFn g_aimHandler = nullptr;
std::atomic<bool> g_gameTimePrecise{false};

// Règle la précision du x87 et renvoie le mot de contrôle d'avant.
unsigned int EnterPrecision(unsigned int precision) noexcept {
    unsigned int previous = 0;
    _controlfp_s(&previous, 0, 0);
    unsigned int ignored = 0;
    _controlfp_s(&ignored, precision, _MCW_PC);
    return previous;
}

// Rétablit seulement le champ de précision : les autres changements faits par le jeu (arrondi,
// exceptions) sont conservés.
void LeavePrecision(unsigned int previous) noexcept {
    unsigned int ignored = 0;
    _controlfp_s(&ignored, previous & _MCW_PC, _MCW_PC);
}

// 0x401F50 : microsecondes = QPC absolu * (float)(1e6 / fréquence), puis conversion entière.
long long __cdecl PreciseMicroseconds() {
    const unsigned int previous = EnterPrecision(_PC_53);
    const long long result = g_microseconds();
    LeavePrecision(previous);
    return result;
}

// 0x701040 : secondes = QPC absolu / fréquence (argument non nul : mémorise la fréquence).
double __cdecl PreciseSeconds(int storeFrequency) {
    const unsigned int previous = EnterPrecision(_PC_53);
    const double result = g_seconds(storeFrequency);
    LeavePrecision(previous);
    return result;
}

// 0x53B8B0, gestion de la caméra de visée (mode 2). Elle borne un angle cible stocké en float puis le
// compare par égalité stricte à la borne encore présente dans un registre x87 : en double précision, les
// deux valeurs diffèrent et la caméra ne suit plus le réticule au bord de l'écran. Mécanisme et
// correctif établis par ZachFix (h714je, GPL-3.0, gameplay/aim_fpu_fix.cpp).
void __cdecl AimHandlerSinglePrecision() {
    const unsigned int previous = EnterPrecision(_PC_24);
    g_aimHandler();
    LeavePrecision(previous);
}

struct Target {
    const char* name;
    std::uintptr_t rva;
    std::span<const unsigned char> signature;  // premiers octets attendus à cette adresse
    const char* testExport;                    // fonction équivalente exportée par le faux jeu des tests
    void* detour;
    void** original;
};

// Débuts des fonctions de DP.exe 1.01b Steam (lus dans l'exécutable, appels par l'IAT compris).
constexpr unsigned char kMicrosecondsSignature[] = {0x83, 0xEC, 0x10, 0x8D, 0x04, 0x24, 0x50,
                                                    0xFF, 0x15, 0x78, 0xE0, 0x76, 0x00};
constexpr unsigned char kSecondsSignature[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x83, 0x7D, 0x08, 0x00};
constexpr unsigned char kAimHandlerSignature[] = {0x83, 0xEC, 0x24, 0x56, 0x8B, 0x35, 0xA4, 0x1E, 0xBE, 0x00};

bool InitializeMinHook() {
    const MH_STATUS status = MH_Initialize();
    return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

// Renvoie l'adresse à intercepter : dans DP.exe 1.01b si la signature correspond, sinon la fonction
// exportée par le faux jeu des tests (DP.exe n'exporte rien).
void* Locate(HMODULE gameModule, const Target& target, bool knownBuild) {
    if (knownBuild) {
        auto* address = reinterpret_cast<unsigned char*>(gameModule) + target.rva;
        if (std::memcmp(address, target.signature.data(), target.signature.size()) == 0) {
            return address;
        }
        log::Warn("{} : code inattendu à {}, correctif non appliqué", target.name,
                  sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(address)));
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(gameModule, target.testExport));
}

bool Hook(HMODULE gameModule, const Target& target, bool knownBuild) {
    void* address = Locate(gameModule, target, knownBuild);
    if (address == nullptr) {
        return false;
    }
    const MH_STATUS created = MH_CreateHook(address, target.detour, target.original);
    const MH_STATUS enabled = created == MH_OK ? MH_EnableHook(address) : created;
    if (enabled != MH_OK) {
        log::Warn("{} : interception impossible (MinHook {})", target.name, static_cast<int>(enabled));
        return false;
    }
    log::Info("{} : correctif appliqué à {}", target.name,
              sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(address)));
    return true;
}

}  // namespace

void Install(HMODULE gameModule) {
    const Config& config = GetConfig();
    if (!config.preciseGameTime && !config.aimPrecisionGuard) {
        log::Info("Précision du x87 par fonction : désactivée");
        return;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(gameModule);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const unsigned char*>(gameModule) +
                                                               dos->e_lfanew);
    const bool knownBuild = nt->FileHeader.TimeDateStamp == kSteam101bTimestamp;
    if (!InitializeMinHook()) {
        log::Warn("MinHook indisponible : précision du x87 par fonction non appliquée");
        return;
    }

    const Target microseconds{"Temps du jeu en microsecondes (double précision)", 0x1F50, kMicrosecondsSignature,
                              "DpsfTestGameMicroseconds", reinterpret_cast<void*>(&PreciseMicroseconds),
                              reinterpret_cast<void**>(&g_microseconds)};
    const Target seconds{"Temps du jeu en secondes (double précision)", 0x301040, kSecondsSignature,
                         "DpsfTestGameSeconds", reinterpret_cast<void*>(&PreciseSeconds),
                         reinterpret_cast<void**>(&g_seconds)};
    const Target aim{"Caméra de visée en simple précision (visée restreinte)", 0x13B8B0, kAimHandlerSignature,
                     "DpsfTestAimHandler", reinterpret_cast<void*>(&AimHandlerSinglePrecision),
                     reinterpret_cast<void**>(&g_aimHandler)};

    if (config.preciseGameTime) {
        const bool microsecondsHooked = Hook(gameModule, microseconds, knownBuild);
        const bool secondsHooked = Hook(gameModule, seconds, knownBuild);
        g_gameTimePrecise.store(microsecondsHooked && (secondsHooked || !knownBuild));
    }
    if (config.aimPrecisionGuard) {
        Hook(gameModule, aim, knownBuild);
    }
}

bool GameTimePrecise() noexcept {
    return g_gameTimePrecise.load();
}

}  // namespace dpsf::patches
