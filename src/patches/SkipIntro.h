#pragma once

#include <windows.h>

namespace dpsf::patches {

// Saute les logos et l'introduction au lancement, comme la modification d'octet connue de la communauté
// (PCGamingWiki, guide Steam 615264656 : DP.exe, offset 0x243333, B3 -> 00), mais en mémoire : DP.exe
// n'est pas modifié sur le disque. En 0x643F2D, `mov dword [0x14736D8], 0xB3` choisit l'étape de départ
// de la séquence de lancement ; 0 démarre directement à l'écran titre.
// Écriture d'un octet après vérification du code environnant : sûr dans DllMain, avant que le jeu ne
// l'exécute.
void ApplySkipIntro(HMODULE gameModule);

}  // namespace dpsf::patches
