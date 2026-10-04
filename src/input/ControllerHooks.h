#pragma once

#include <windows.h>

namespace dpsf::input {

// Intercepte winmm!joyGetPosEx dans DP.exe, seule fonction par laquelle le jeu lit les manettes (vérifié :
// IAT 0x76E250, lecture en 0x709D95). Pour une manette Sony, l'état est présenté avec la disposition
// Xbox 360 attendue par le jeu (voir ControllerMapping.h). Le journal décrit chaque manette et, en mode
// diagnostic, ses axes bruts quand ils bougent.
void Install(HMODULE gameModule);

}  // namespace dpsf::input
