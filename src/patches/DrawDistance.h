#pragma once

#include <windows.h>

namespace dpsf::patches {

// Distance d'affichage des classes d'objets (option [Graphics] DrawDistanceScale, 1 = jeu d'origine).
// La caméra de DP.exe 1.01b (0x6B62E0) construit 6 frustums de culling, un par classe de distance, avec
// les plans lointains 200000, 80000, 20000, 5000, 1000 et 500 (voir docs/analyse-dp-exe.md, « Caméra :
// projection et classes de distance »). Les 6 valeurs sont chargées par des `fld dword ptr [constante]`
// en 0x6B654E..0x6B658A. On redirige l'opérande des classes 1 à 5 vers nos propres valeurs, multipliées
// par le réglage : les constantes de .rdata, partagées avec d'autres codes, ne sont pas modifiées.
// La classe 0 (200000, déjà au plan lointain de la projection principale) est laissée telle quelle.
// Écriture de 5 opérandes après vérification des 72 octets de la boucle : sûr dans DllMain.
void ApplyDrawDistance(HMODULE gameModule);

}  // namespace dpsf::patches
