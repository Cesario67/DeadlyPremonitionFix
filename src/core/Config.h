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

    // [Graphics]
    // DPfix (Durante) intégré et corrigé : résolution, SMAA, SSAO... réglés dans DPfix.ini. Désactivé
    // automatiquement si un d3d9.dll externe (DPfix d'origine) est présent dans le dossier du jeu.
    bool integratedDpfix = true;

    // [Controller]
    // Manette Sony (DualSense, DS4) présentée au jeu avec la disposition Xbox 360 qu'il attend : sans cela,
    // la gâchette L2 est lue comme stick droit et la caméra tourne sans fin.
    bool sonyControllerLayout = true;
    // Journalise chaque manette et ses axes bruts quand ils bougent (vérification de la correspondance).
    bool controllerDiagnostics = true;
    bool swapControllerTriggers = false;

    // [Compat]
    // Si Reset échoue avec DPfix installé, fait relâcher à DPfix ses références de surfaces puis
    // retente (blocage après alt-tab en plein écran, voir graphics/D3D9Hooks.cpp).
    bool dpfixResetWorkaround = true;

    // [Debug]
    bool logAllFileOpens = false;
    int logKeepCount = 10;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace dpsf
