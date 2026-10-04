#include "input/ControllerMapping.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace dpsf::input {

namespace {

constexpr WORD kSonyVendorId = 0x054C;
constexpr DWORD kAxisMax = 65535;
constexpr DWORD kAxisCenter = 32767;

// Ramène une valeur de [minimum, maximum] sur [0, 65535].
DWORD Normalize(DWORD value, DWORD minimum, DWORD maximum) noexcept {
    if (maximum <= minimum) {
        return value;
    }
    const DWORD clamped = std::clamp(value, minimum, maximum);
    return static_cast<DWORD>((static_cast<std::uint64_t>(clamped - minimum) * kAxisMax) / (maximum - minimum));
}

// Indices (base 0) des boutons Sony à placer en position Xbox 0, 1, 2... :
// A <- Croix, B <- Rond, X <- Carré, Y <- Triangle, LB <- L1, RB <- R1, Back <- Create, Start <- Options,
// LS <- L3, RS <- R3.
constexpr std::array<int, 10> kXboxFromSonyButton = {1, 2, 0, 3, 4, 5, 8, 9, 10, 11};

}  // namespace

ControllerFamily IdentifyController(const JOYCAPSW& caps) noexcept {
    // Pour une manette HID, WinMM renseigne wMid / wPid avec l'identifiant USB du fabricant / produit.
    return caps.wMid == kSonyVendorId ? ControllerFamily::Sony : ControllerFamily::Other;
}

AxisRanges RangesFromCaps(const JOYCAPSW& caps) noexcept {
    return {caps.wXmin, caps.wXmax, caps.wYmin, caps.wYmax, caps.wZmin, caps.wZmax,
            caps.wRmin, caps.wRmax, caps.wUmin, caps.wUmax, caps.wVmin, caps.wVmax};
}

void SonyToXboxLayout(JOYINFOEX& state, const AxisRanges& ranges, bool swapTriggers) noexcept {
    const DWORD leftX = Normalize(state.dwXpos, ranges.xMin, ranges.xMax);
    const DWORD leftY = Normalize(state.dwYpos, ranges.yMin, ranges.yMax);
    const DWORD rightX = Normalize(state.dwZpos, ranges.zMin, ranges.zMax);
    const DWORD rightY = Normalize(state.dwRpos, ranges.rMin, ranges.rMax);
    // Mesuré sur une DualSense : L2 sur V (avec le bouton 6), R2 sur U (avec le bouton 7).
    const DWORD l2 = Normalize(state.dwVpos, ranges.vMin, ranges.vMax);
    const DWORD r2 = Normalize(state.dwUpos, ranges.uMin, ranges.uMax);

    // Xbox 360 sous WinMM : un seul axe Z pour les deux gâchettes, au centre au repos.
    const long triggers = (static_cast<long>(l2) - static_cast<long>(r2)) / 2 * (swapTriggers ? -1 : 1);
    state.dwXpos = leftX;
    state.dwYpos = leftY;
    state.dwZpos = static_cast<DWORD>(std::clamp<long>(static_cast<long>(kAxisCenter) + triggers, 0, kAxisMax));
    state.dwRpos = rightY;
    state.dwUpos = rightX;
    state.dwVpos = 0;  // axe inexistant sur une manette Xbox 360

    DWORD buttons = 0;
    DWORD firstPressed = 0;  // dwButtonNumber : numéro (base 1) d'un bouton enfoncé, 0 si aucun
    for (size_t xbox = 0; xbox < kXboxFromSonyButton.size(); ++xbox) {
        if ((state.dwButtons & (1u << kXboxFromSonyButton[xbox])) != 0) {
            buttons |= 1u << xbox;
            if (firstPressed == 0) {
                firstPressed = static_cast<DWORD>(xbox + 1);
            }
        }
    }
    state.dwButtons = buttons;
    state.dwButtonNumber = firstPressed;
    // dwPOV (croix directionnelle) : même convention des deux côtés.
}

}  // namespace dpsf::input
