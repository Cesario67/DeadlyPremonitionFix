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
    // Limiteur d'images (0 = aucun). 60 par défaut : le jeu avance à la vitesse d'affichage, et PhysX comme
    // certains effets supposent une cadence proche de 60 i/s (recherches de ZachFix, research/engine/timing.md).
    int frameLimitFps = 60;
    // Fonctions de temps de DP.exe calculées en double précision (voir patches/FpuPatches.h) : sans cela,
    // Direct3D 9 laisse le x87 en simple précision et la résolution du temps du jeu se dégrade avec la
    // durée écoulée depuis le démarrage du PC (voir docs/analyse-dp-exe.md).
    bool preciseGameTime = true;
    // Ajoute D3DCREATE_FPU_PRESERVE à la création du périphérique : double précision pour tout le jeu.
    // Constaté en jeu le 04/10/2026 : en simple précision (celle du jeu d'origine), la caméra saccade quand
    // on la tourne, même à cadence d'images régulière ; en double précision, elle est fluide. La visée,
    // seule fonction connue qui exige la simple précision (ZachFix), y est remise par AimPrecisionGuard.
    bool forceFpuPreserve = true;

    // [Gameplay]
    // Gestion de la caméra de visée toujours exécutée en simple précision (voir patches/FpuPatches.h).
    bool aimPrecisionGuard = true;
    // Logos et introduction sautés au lancement (voir patches/SkipIntro.h).
    bool skipIntro = true;
    // Division par zéro du calcul de vitesse évitée (voir patches/ZeroDeltaGuard.h).
    bool zeroDeltaGuard = true;

    // [Graphics]
    // DPfix (Durante) intégré et corrigé : résolution, SMAA, SSAO... réglés dans DPfix.ini. Désactivé
    // automatiquement si un d3d9.dll externe (DPfix d'origine) est présent dans le dossier du jeu.
    bool integratedDpfix = true;

    // [Controller]
    // Manette Sony (DualSense, DS4) présentée au jeu avec la disposition Xbox 360 qu'il attend : sans cela,
    // la gâchette R2 (axe U, 0 au repos) est lue comme stick droit poussé à fond et la caméra tourne
    // sans fin.
    bool sonyControllerLayout = true;
    // Journalise chaque manette et ses axes bruts quand ils bougent (vérification de la correspondance).
    bool controllerDiagnostics = true;
    bool swapControllerTriggers = false;
    // En jeu, toutes les 20 s, un appel à joyGetPosEx bloque ~64 ms dans WinMM (réénumération des
    // périphériques, mesuré le 04/10/2026) : saccade régulière. Les manettes sont lues par un thread à part
    // et le jeu reçoit instantanément le dernier état lu.
    bool backgroundControllerPolling = true;

    // [Compat]
    // Si Reset échoue avec DPfix installé, fait relâcher à DPfix ses références de surfaces puis
    // retente (blocage après alt-tab en plein écran, voir graphics/D3D9Hooks.cpp).
    bool dpfixResetWorkaround = true;

    // [Debug]
    bool logAllFileOpens = false;
    // Relevé en lecture seule des appels de rendu de DP.exe (projections, cibles, viewports) : voir
    // graphics/RenderDiagnostics.h. Verbeux et un peu coûteux : à n'activer que pour une analyse.
    bool renderDiagnostics = false;
    int logKeepCount = 10;
};

// Retourne false si le fichier .ini est absent (les valeurs par défaut sont alors utilisées).
bool LoadConfig(const std::wstring& iniPath);

const Config& GetConfig() noexcept;

}  // namespace dpsf
