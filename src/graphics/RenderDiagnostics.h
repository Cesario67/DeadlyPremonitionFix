#pragma once

#include <d3d9.h>

namespace dpsf::graphics::diag {

// Diagnostic de rendu, en lecture seule (option [Debug] RenderDiagnostics) : relève ce que DP.exe envoie à
// Direct3D pour trouver quelles distances (plans proche et lointain de la projection, cibles et viewports
// des ombres) chaque passe utilise, et quel code du jeu les fixe (adresse de l'appelant).
// Aucun appel ne modifie le comportement : les hooks transmettent toujours à la fonction d'origine.
//
// Les fonctions sont appelées depuis les hooks de IDirect3DDevice9 (D3D9Hooks.cpp), potentiellement depuis
// plusieurs threads : l'état est protégé par un verrou et borné (table de taille fixe, pas d'allocation
// dans le chemin chaud hors première observation).
void OnSetTransform(const void* caller, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) noexcept;
void OnSetVertexShaderConstants(const void* caller, UINT startRegister, const float* data, UINT vector4Count) noexcept;
void OnSetRenderTarget(const void* caller, DWORD index, IDirect3DSurface9* surface) noexcept;
void OnSetViewport(const void* caller, const D3DVIEWPORT9* viewport) noexcept;

// Appelé à chaque Present : écrit le bilan de la fenêtre dans le journal à chaque intervalle de rapport.
void OnPresent() noexcept;

}  // namespace dpsf::graphics::diag
