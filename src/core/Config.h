#pragma once

#include <string>

namespace dpsf {

// Reflet de DPStabilityFix.ini. Les valeurs par défaut s'appliquent si le fichier ou une clé manque.
struct Config {
    // [Save]
    bool saveGuard = true;          // sauvegardes de secours + journalisation des écritures de dp.sav
    bool atomicSaveWrites = true;   // écrire dans un fichier temporaire puis remplacer dp.sav d'un bloc
    int saveBackupCount = 20;       // nombre de copies de secours conservées
    std::wstring saveFile = L"savedata\\dp.sav";

    // [Crash]
    bool crashDumps = true;
    bool fullMemoryDumps = false;   // dump complet (jusqu'à 2 Go) au lieu d'un dump réduit
    int dumpKeepCount = 10;

    // [Frames]
    bool frameStats = true;         // mesures de cadence, de Sleep et de mémoire dans le journal
    int reportIntervalSeconds = 10;
    int timerResolutionMs = 1;      // 0 = ne pas modifier la résolution du minuteur Windows
    int frameLimitFps = 0;          // 0 = pas de limiteur (le jeu gère sa propre cadence)
    // Ajoute D3DCREATE_FPU_PRESERVE à la création du périphérique : sans lui, Direct3D 9 passe le
    // x87 du thread de rendu en simple précision, ce qui dégrade les calculs de temps de DP.exe
    // (voir docs/analyse-dp-exe.md).
    bool forceFpuPreserve = true;

    // [Debug]
    bool logAllFileOpens = false;
    int logKeepCount = 10;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace dpsf
