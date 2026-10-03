#pragma once

#include <d3d9.h>

// Interface entre DPStabilityFix et DPfix (Durante, third_party/dpfix), compilé dans la même DLL.
// Ce fichier ne dépend pas des en-têtes de DPfix : le reste du mod n'utilise que ces fonctions.
namespace dpsf::dpfix {

// Charge DPfix.ini et DPfixKeys.ini depuis le dossier du jeu. Renvoie false (DPfix inactif) si les
// shaders indispensables (dossier dpfix\ à côté de DP.exe) sont absents.
bool Initialize();

// Enveloppe l'objet Direct3D du système dans celui de DPfix (hkIDirect3D9), comme le faisait son
// d3d9.dll. Les périphériques créés ensuite sont eux aussi enveloppés.
IDirect3D9* Wrap(IDirect3D9* direct3d);

}  // namespace dpsf::dpfix
