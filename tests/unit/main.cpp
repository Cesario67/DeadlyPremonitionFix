// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "graphics/ProjectionAnalysis.h"
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
    // Le défaut corrigé : R2 relâchée (U = 0) était lue par le jeu comme stick droit poussé à fond.
    Check(state.dwUpos == 32767, "stick droit horizontal au centre (et non plus R2 à 0)");
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
    state.dwVpos = 65535;  // L2 enfoncée
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos > 60000, "L2 vers un extrême de Z");
    state = Neutral();
    state.dwUpos = 65535;  // R2 enfoncée
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos < 5000, "R2 vers l'autre extrême de Z");
    state = Neutral();
    state.dwUpos = 65535;
    dpsf::input::SonyToXboxLayout(state, ranges, true);
    Check(state.dwZpos > 60000, "gâchettes inversées sur demande");
}

// Valeurs relevées sur une vraie DualSense (04/10/2026, Bluetooth) avec joyGetPosEx.
void TestMeasuredDualSense() {
    std::printf("Mesures réelles DualSense\n");
    const dpsf::input::AxisRanges ranges = dpsf::input::RangesFromCaps(SonyCaps());
    auto measured = [](DWORD x, DWORD y, DWORD z, DWORD r, DWORD u, DWORD v, DWORD buttons) {
        JOYINFOEX state = Neutral();
        state.dwXpos = x;
        state.dwYpos = y;
        state.dwZpos = z;
        state.dwRpos = r;
        state.dwUpos = u;
        state.dwVpos = v;
        state.dwButtons = buttons;
        return state;
    };
    auto nearCenter = [](DWORD value) { return value > 32767 - 4000 && value < 32767 + 4000; };

    JOYINFOEX state = measured(31743, 32768, 32511, 32767, 0, 0, 0);  // au repos
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(nearCenter(state.dwUpos) && nearCenter(state.dwRpos) && nearCenter(state.dwZpos),
          "au repos : stick droit et gâchettes au centre pour le jeu");

    state = measured(31743, 32768, 17919, 32768, 0, 0, 0);  // stick droit vers la gauche
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwUpos < 20000 && nearCenter(state.dwRpos), "stick droit à gauche : U diminue");

    state = measured(31743, 32768, 32768, 46551, 0, 0, 0);  // stick droit vers le bas
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwRpos > 45000 && nearCenter(state.dwUpos), "stick droit vertical : R seul bouge");

    state = measured(31743, 32768, 32511, 32767, 0, 49932, 0x0040);  // L2 enfoncée (V + bouton 6)
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos > 50000 && nearCenter(state.dwUpos) && state.dwButtons == 0,
          "L2 : Z vers LT, stick droit immobile, bouton numérique ignoré");

    state = measured(31743, 32768, 32511, 32767, 48111, 0, 0x0080);  // R2 enfoncée (U + bouton 7)
    dpsf::input::SonyToXboxLayout(state, ranges, false);
    Check(state.dwZpos < 15000 && nearCenter(state.dwUpos) && state.dwButtons == 0,
          "R2 : Z vers RT, stick droit immobile, bouton numérique ignoré");
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


void FillPerspective(float (&m)[16], bool rightHanded, float fovYDegrees, float aspect, float zn, float zf) {
    std::memset(m, 0, sizeof(m));
    const float yScale = 1.0f / std::tan(fovYDegrees * 3.14159265f / 180.0f / 2.0f);
    m[0] = yScale / aspect;
    m[5] = yScale;
    m[10] = rightHanded ? zf / (zn - zf) : zf / (zf - zn);
    m[11] = rightHanded ? -1.0f : 1.0f;
    m[14] = rightHanded ? zn * zf / (zn - zf) : -zn * zf / (zf - zn);
}

void Transpose(float (&m)[16]) {
    for (int row = 0; row < 4; ++row) {
        for (int column = row + 1; column < 4; ++column) {
            std::swap(m[row * 4 + column], m[column * 4 + row]);
        }
    }
}

bool Near(float value, float expected, float relative) {
    return std::fabs(value - expected) <= relative * std::fabs(expected) + 1e-3f;
}

void TestProjectionAnalysis() {
    std::printf("Analyse des projections\n");
    using dpsf::graphics::AnalyzeProjection;
    using dpsf::graphics::ProjectionKind;
    float m[16];
    FillPerspective(m, false, 60.0f, 16.0f / 9.0f, 0.5f, 1234.5f);
    dpsf::graphics::ProjectionInfo info = AnalyzeProjection(m);
    Check(info.kind == ProjectionKind::PerspectiveLH && !info.transposed, "perspective LH reconnue");
    Check(Near(info.nearPlane, 0.5f, 0.01f) && Near(info.farPlane, 1234.5f, 0.01f), "plans proche et lointain LH");
    Check(Near(info.fovY, 60.0f, 0.01f) && Near(info.aspect, 16.0f / 9.0f, 0.01f), "champ de vision et format");

    FillPerspective(m, true, 45.0f, 1.5f, 1.0f, 800.0f);
    info = AnalyzeProjection(m);
    Check(info.kind == ProjectionKind::PerspectiveRH, "perspective RH reconnue");
    Check(Near(info.nearPlane, 1.0f, 0.01f) && Near(info.farPlane, 800.0f, 0.01f), "plans proche et lointain RH");

    FillPerspective(m, false, 60.0f, 1.0f, 0.5f, 3000.0f);
    Transpose(m);
    info = AnalyzeProjection(m);
    Check(info.kind == ProjectionKind::PerspectiveLH && info.transposed, "perspective transposée (constante de shader)");
    Check(Near(info.farPlane, 3000.0f, 0.02f), "lointain d'une projection transposée");

    float ortho[16] = {};
    ortho[0] = 0.02f;
    ortho[5] = 0.02f;
    ortho[10] = 1.0f / 499.0f;
    ortho[14] = -1.0f / 499.0f;
    ortho[15] = 1.0f;
    info = AnalyzeProjection(ortho);
    Check(info.kind == ProjectionKind::OrthoLH && Near(info.nearPlane, 1.0f, 0.01f) && Near(info.farPlane, 500.0f, 0.01f),
          "orthographique LH (ombres) : proche 1, lointain 500");

    float combined[16] = {0.8f, 0.1f, 0.3f, 0.2f, -0.2f, 0.9f, 0.1f, 0.4f, 0.3f, 0.1f, 1.0f, 1.0f, 5.0f, 2.0f, -3.0f, 0.5f};
    Check(AnalyzeProjection(combined).kind == ProjectionKind::None, "matrice combinée : non reconnue");
}

}  // namespace

int main() {
    TestIdentification();
    TestAxesAtRest();
    TestAxesMoved();
    TestMeasuredDualSense();
    TestCustomRanges();
    TestButtons();
    TestProjectionAnalysis();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
