#pragma once

// Modifié pour DPStabilityFix : bibliothèques liées par CMake (d3dx9, dxguid). d3d9.lib n'est plus liée
// (la DLL ne doit pas importer d3d9.dll elle-même) et dxerr.lib n'existe plus dans les SDK actuels.

#include <d3d9.h>
#include <d3dx9.h>
#include "d3d9int.h"
#include "d3d9dev.h"

// Modifié pour DPStabilityFix : plus d'export Direct3DCreate9, l'enveloppe est créée par DpfixBridge.
