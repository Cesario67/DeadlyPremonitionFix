// Point d'entrée de DPStabilityFix.
//
// DllMain s'exécute sous le verrou du chargeur de Windows, avant le code du jeu : on n'y fait que des
// opérations sûres (fichiers, écriture de pointeurs dans l'IAT de DP.exe). Ce qui demande de charger
// d'autres DLL est reporté au premier appel du jeu à Direct3D (voir graphics/D3D9Hooks.cpp).

#include <windows.h>

#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"
#include "core/SystemInfo.h"
#include "crash/CrashHandler.h"
#include "framepacing/FrameMonitor.h"
#include "graphics/D3D9Hooks.h"
#include "input/ControllerHooks.h"
#include "patches/SkipIntro.h"
#include "patches/ZeroDeltaGuard.h"
#include "save/SaveGuard.h"

namespace {

bool g_active = false;

bool IsHostedByGame() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring_view name(path);
    const size_t separator = name.find_last_of(L"\\/");
    if (separator != std::wstring_view::npos) {
        name.remove_prefix(separator + 1);
    }
    return dpsf::EqualsIgnoreCase(name, L"DP.exe");
}

void Startup() {
    // Chargée par un autre programme : on reste un simple proxy de X3DAudio.
    if (!IsHostedByGame() || !dpsf::InitPaths()) {
        return;
    }
    const dpsf::Paths& paths = dpsf::GetPaths();
    const bool iniFound = dpsf::LoadConfig(paths.iniFile);
    if (!dpsf::log::Open(paths.logsDir, dpsf::GetConfig().logKeepCount)) {
        return;
    }
    g_active = true;

    dpsf::log::Info("DPStabilityFix {} : démarrage", DPSF_VERSION);
    if (!iniFound) {
        dpsf::log::Warn("{} absent : valeurs par défaut utilisées", dpsf::WideToUtf8(paths.iniFile));
    }

    const HMODULE game = GetModuleHandleW(nullptr);
    dpsf::sysinfo::LogStartupInfo(game);
    dpsf::crash::Install(game);
    dpsf::save::Install(game);
    dpsf::frames::Install(game);
    dpsf::graphics::Install(game);
    dpsf::input::Install(game);
    dpsf::patches::ApplySkipIntro(game);
    dpsf::patches::ApplyZeroDeltaGuard(game);
    dpsf::log::Info("Initialisation terminée");
}

void Shutdown(bool processTerminating) {
    if (!g_active) {
        return;
    }
    if (processTerminating) {
        dpsf::save::OnProcessExit();
        dpsf::frames::LogFinalReport();
        dpsf::log::Info("Fermeture normale du jeu");
    }
    dpsf::log::Close();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            try {
                Startup();
            } catch (...) {
                // Une erreur d'initialisation ne doit pas empêcher le jeu de démarrer.
            }
            break;
        case DLL_PROCESS_DETACH:
            try {
                Shutdown(reserved != nullptr);
            } catch (...) {
            }
            break;
        default:
            break;
    }
    return TRUE;
}
