#pragma once

#include <windows.h>

namespace dpsf::patches {

// Précision du x87 réglée fonction par fonction dans DP.exe (voir docs/analyse-dp-exe.md, « Temps du jeu
// et précision du x87 ») :
//   - temps du jeu (0x401F50, 0x701040) calculé en double précision : sinon sa résolution se dégrade avec
//     la durée écoulée depuis le démarrage du PC (65 ms après une semaine) ;
//   - gestion de la visée (0x53B8B0) en simple précision : en double précision, la caméra ne suit plus
//     le réticule au bord de l'écran (visée restreinte). Contournement du symptôme observé par ZachFix ;
//     la cause racine n'est pas établie.
// À appeler au premier Direct3DCreate9 (MinHook est interdit dans DllMain), donc avant CreateDevice.
void Install(HMODULE gameModule);

// Vrai si les fonctions de temps de DP.exe sont calculées en double précision.
bool GameTimePrecise() noexcept;

}  // namespace dpsf::patches
