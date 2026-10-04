// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <cstdio>

#include "input/ControllerMapping.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* name) {
    std::printf("  %s %s\n", condition ? "OK   " : "ECHEC", name);
    if (!condition) {
        ++g_failures;
    }
}

JOYCAPSW SonyCaps() {
    JOYCAPSW caps{};
    caps.wMid = 0x054C;  // Sony
    caps.wPid = 0x0CE6;  // DualSense
    caps.wXmax = caps.wYmax = caps.wZmax = caps.wRmax = caps.wUmax = caps.wVmax = 65535;
    return caps;
}

JOYINFOEX Neutral() {
    JOYINFOEX state{};
    state.dwSize = sizeof(state);
    state.dwFlags = JOY_RETURNALL;
    state.dwXpos = state.dwYpos = state.dwZpos = state.dwRpos = 32767;  // sticks au centre
    state.dwUpos = state.dwVpos = 0;                                    // gâchettes relâchées
    state.dwPOV = JOY_POVCENTERED;
    return state;
}

void TestIdentification() {
    std::printf("Identification\n");
    JOYCAPSW sony = SonyCaps();
    Check(dpsf::input::IdentifyController(sony) == dpsf::input::ControllerFamily::Sony, "DualSense reconnue");
    JOYCAPSW xbox{};
    xbox.wMid = 0x045E;  // Microsoft
    Check(dpsf::input::IdentifyController(xbox) == dpsf::input::ControllerFamily::Other, "manette Xbox laissée telle quelle");
}

void TestAxesAtRest() {
    std::printf("Axes au repos\n");
    JOYINFOEX state = Neutral();
    dpsf::input::SonyToXboxLayout(state, dpsf::input::RangesFromCaps(SonyCaps()), false);
    // Le défaut corrigé : L2 relâchée (U = 0) était lue par le jeu comme stick droit poussé à fond.
    Check(state.dwUpos == 32767, "stick droit horizontal au centre (et non plus L2 à 0)");
    Check(state.dwZpos == 32767, "gâchettes combinées au centre au repos");
    Check(state.dwRpos == 32767, "stick droit vertical au centre");
    Check(state.dwVpos == 0, "axe V inutilisé");
}

void TestAxesMoved() {
    std::printf("Axes en mouvement\n");
    const dpsf::input::AxisRanges ranges = dpsf::input::RangesFromCaps(SonyCaps());
    JOYINFOEX state = Neutral();
    state.dwZpos = 65535;  // stick droit à fond à droite (Sony : Z)
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwUpos == 65535, "stick droit horizontal transmis sur U");

    state = Neutral();
    state.dwUpos = 65535;  // L2 enfoncée
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos > 60000, "L2 vers un extrême de Z");
    state = Neutral();
    state.dwVpos = 65535;  // R2 enfoncée
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos < 5000, "R2 vers l'autre extrême de Z");
    state = Neutral();
    state.dwVpos = 65535;
    dpsf::input::SonyToXboxLayout(state, ranges, true);
    Check(state.dwZpos > 60000, "gâchettes inversées sur demande");
}

void TestCustomRanges() {
    std::printf("Plages non standard\n");
    JOYCAPSW caps = SonyCaps();
    caps.wZmin = 0;
    caps.wZmax = 255;  // pilote qui annonce une plage 0-255
    JOYINFOEX state = Neutral();
    state.dwXpos = 32767;
    state.dwZpos = 255;
    dpsf::input::SonyToXboxLayout(state, dpsf::input::RangesFromCaps(caps), false);
    Check(state.dwUpos == 65535, "valeur ramenée sur 0-65535");
}

void TestButtons() {
    std::printf("Boutons\n");
    const dpsf::input::AxisRanges ranges = dpsf::input::RangesFromCaps(SonyCaps());
    struct Case {
        int sony;
        int xbox;
        const char* name;
    };
    const Case cases[] = {
        {1, 0, "Croix -> A"},   {2, 1, "Rond -> B"},     {0, 2, "Carré -> X"},    {3, 3, "Triangle -> Y"},
        {4, 4, "L1 -> LB"},     {5, 5, "R1 -> RB"},      {8, 6, "Create -> Back"}, {9, 7, "Options -> Start"},
        {10, 8, "L3 -> LS"},    {11, 9, "R3 -> RS"},
    };
    for (const Case& test : cases) {
        JOYINFOEX state = Neutral();
        state.dwButtons = 1u << test.sony;
        dpsf::input::SonyToXboxLayout(state, ranges, false);
        Check(state.dwButtons == (1u << test.xbox) && state.dwButtonNumber == static_cast<DWORD>(test.xbox + 1),
              test.name);
    }
    JOYINFOEX state = Neutral();
    state.dwButtons = 1u << 12;  // PS : sans équivalent Xbox, ne doit pas déclencher d'action
    state.dwPOV = 9000;
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwButtons == 0 && state.dwButtonNumber == 0, "bouton PS ignoré");
    Check(state.dwPOV == 9000, "croix directionnelle conservée");
}

}  // namespace

int main() {
    TestIdentification();
    TestAxesAtRest();
    TestAxesMoved();
    TestCustomRanges();
    TestButtons();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
