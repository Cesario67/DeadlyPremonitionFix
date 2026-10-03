#pragma once

#include <string>

namespace dpsf {

struct Paths {
    std::wstring gameExe;     // ...\DP.exe
    std::wstring gameDir;     // dossier du jeu
    std::wstring iniFile;     // ...\DPStabilityFix.ini
    std::wstring modDir;      // ...\DPStabilityFix
    std::wstring logsDir;     // ...\DPStabilityFix\logs
    std::wstring dumpsDir;    // ...\DPStabilityFix\crashdumps
    std::wstring backupsDir;  // ...\DPStabilityFix\savebackups
};

// Calcule les chemins à partir de l'exécutable hôte et crée les dossiers du mod.
bool InitPaths();

const Paths& GetPaths() noexcept;

}  // namespace dpsf
