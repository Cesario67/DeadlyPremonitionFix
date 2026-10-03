// Pont vers DPfix : remplace son main.cpp (DllMain, journal, chemins) et son d3d9.cpp (export
// Direct3DCreate9), qui ne sont pas compilés. Les fonctions globales déclarées dans main.h sont
// définies ici.

#include "DpfixBridge.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/Log.h"
#include "dpfix/src/KeyActions.h"
#include "dpfix/src/Settings.h"
#include "dpfix/src/main.h"

// --- Fonctions attendues par DPfix (main.h) -------------------------------------------------------

tDirect3DCreate9 oDirect3DCreate9 = nullptr;  // inutilisé : l'objet est créé par DPStabilityFix

namespace {

char g_moduleDirectory[MAX_PATH] = {};  // dossier de la DLL (= dossier du jeu), avec '\' final

}  // namespace

char* GetDirectoryFile(char* filename) {
    static char path[MAX_PATH];
    strcpy_s(path, g_moduleDirectory);
    strcat_s(path, filename);
    return path;
}

bool fileExists(const char* filename) {
    return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
}

void __cdecl sdlogtime() {}

void __cdecl sdlog(const char* format, ...) {
    if (format == nullptr) {
        return;
    }
    char buffer[2048];
    va_list arguments;
    va_start(arguments, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, arguments);
    va_end(arguments);
    std::string line = std::string("[DPfix] ") + buffer;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    dpsf::log::Write(dpsf::log::Level::Info, line);
}

void errorExit(LPTSTR function) {
    const DWORD error = GetLastError();
    char message[512];
    sprintf_s(message, "DPfix : %s a échoué (erreur %lu).", function, error);
    dpsf::log::Write(dpsf::log::Level::Error, message);
    MessageBoxA(nullptr, message, "DPStabilityFix", MB_OK | MB_ICONERROR);
    ExitProcess(error);
}

// --- Interface DPStabilityFix ---------------------------------------------------------------------

namespace dpsf::dpfix {

bool Initialize() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&Initialize), &self);
    GetModuleFileNameA(self, g_moduleDirectory, MAX_PATH);
    if (char* separator = strrchr(g_moduleDirectory, '\\')) {
        separator[1] = '\0';
    }

    // SMAA est l'effet de base ; sans lui (et sans le dossier dpfix\), DPfix ne doit pas démarrer.
    if (!fileExists(GetDirectoryFile(const_cast<char*>("dpfix\\SMAA.fx")))) {
        log::Warn("DPfix intégré inactif : shaders absents ({}dpfix\\)", g_moduleDirectory);
        return false;
    }
    if (!fileExists(GetDirectoryFile(const_cast<char*>(SETTINGS_FILE_NAME)))) {
        log::Warn("DPfix.ini absent : DPfix intégré utilise ses valeurs par défaut");
    }

    Settings::get().load();
    KeyActions::get().load();
    const Settings& settings = Settings::get();
    log::Info("DPfix intégré (0.9 corrigé) : rendu {}x{}, affichage {}x{}, AA {} (SMAA), SSAO {}, "
              "sans bordure {}, fenêtré {}",
              settings.getRenderWidth(), settings.getRenderHeight(), settings.getPresentWidth(),
              settings.getPresentHeight(), settings.getAAQuality(), settings.getSsaoStrength(),
              settings.getBorderlessFullscreen() ? "oui" : "non", settings.getForceWindowed() ? "oui" : "non");
    return true;
}

IDirect3D9* Wrap(IDirect3D9* direct3d) {
    if (direct3d == nullptr) {
        return nullptr;
    }
    // Le constructeur remplace le pointeur reçu par l'enveloppe (même convention que d3d9.cpp).
    new hkIDirect3D9(&direct3d);
    return direct3d;
}

}  // namespace dpsf::dpfix
