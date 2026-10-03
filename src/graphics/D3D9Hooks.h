#pragma once

#include <windows.h>

namespace dpsf::graphics {

// Intercepte Direct3DCreate9 dans DP.exe, puis IDirect3D9::CreateDevice et IDirect3DDevice9::Present
// / Reset par leur vtable. Fonctionne avec ou sans DPfix : on se place devant l'objet que reçoit le jeu.
void Install(HMODULE gameModule);

}  // namespace dpsf::graphics
