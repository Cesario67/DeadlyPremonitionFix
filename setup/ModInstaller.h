#pragma once

#include <string>

#include "Result.h"

namespace setup {

// Copie X3DAudio1_7.dll, DPStabilityFix.ini, DPfix.ini, DPfixKeys.ini et les shaders (dossier dpfix)
// depuis `sourceDir` (dossier de l'installeur) vers `gameDir`. Une X3DAudio1_7.dll étrangère déjà
// présente est mise de côté (.avant-dpsf) ; les .ini existants sont conservés (réglages de l'utilisateur).
Result InstallMod(const std::wstring& gameDir, const std::wstring& sourceDir);

// Un DPfix d'origine (d3d9.dll de Durante) est présent : DPfix intégré reste alors inactif.
bool HasExternalDpfix(const std::wstring& gameDir);

// Renomme d3d9.dll (sans le supprimer) pour laisser la place à DPfix intégré.
Result DisableExternalDpfix(const std::wstring& gameDir);

}  // namespace setup
