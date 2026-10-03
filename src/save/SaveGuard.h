#pragma once

#include <windows.h>

namespace dpsf::save {

// Protège dp.sav contre les plantages pendant l'écriture :
// - copie de secours horodatée avant chaque écriture (rotation) ;
// - écriture atomique : le jeu écrit dans une copie temporaire, qui ne remplace dp.sav qu'une fois
//   le fichier fermé (ou à chaque FlushFileBuffers). Un plantage en cours d'écriture laisse donc
//   l'ancien dp.sav intact.
// Les écritures de sauvegarde sont journalisées pour confirmer le comportement réel du jeu.
void Install(HMODULE gameModule);

// Valide les écritures encore ouvertes quand le processus se termine normalement.
void OnProcessExit() noexcept;

// Décrit l'état des écritures de sauvegarde dans le journal (appelé depuis le gestionnaire de plantage).
void LogCrashState() noexcept;

}  // namespace dpsf::save
