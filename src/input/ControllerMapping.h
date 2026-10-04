#pragma once

#include <windows.h>
#include <mmsystem.h>

namespace dpsf::input {

enum class ControllerFamily {
    Other,
    Sony,  // DualShock 4, DualSense, DualSense Edge (fabricant USB 0x054C)
};

ControllerFamily IdentifyController(const JOYCAPSW& caps) noexcept;

// Plages des 6 axes WinMM d'une manette, lues dans JOYCAPS (souvent 0 à 65535).
struct AxisRanges {
    DWORD xMin, xMax, yMin, yMax, zMin, zMax, rMin, rMax, uMin, uMax, vMin, vMax;
};

AxisRanges RangesFromCaps(const JOYCAPSW& caps) noexcept;

// Présente l'état d'une manette Sony avec la disposition d'une manette Xbox 360 sous WinMM, celle que
// DP.exe attend. Correspondance Sony mesurée le 04/10/2026 sur une DualSense (0x0CE6, Bluetooth) ; côté
// Xbox, disposition WinMM habituelle d'une manette Xbox 360 (non mesurée, pas de manette Xbox) :
//   Sony : X/Y stick gauche, Z stick droit horizontal, R stick droit vertical, U gâchette R2, V gâchette L2
//          (gâchettes à 0 au repos)
//   Xbox : X/Y stick gauche, Z gâchettes combinées (centre au repos), R stick droit vertical,
//          U stick droit horizontal
//   Boutons Sony (Carré, Croix, Rond, Triangle, L1, R1, L2, R2, Create, Options, L3, R3...) remis dans
//   l'ordre Xbox (A, B, X, Y, LB, RB, Back, Start, LS, RS).
// Les axes sont ramenés sur 0 à 65535. `swapTriggers` inverse le sens de Z (L2 / R2).
void SonyToXboxLayout(JOYINFOEX& state, const AxisRanges& ranges, bool swapTriggers) noexcept;

}  // namespace dpsf::input
