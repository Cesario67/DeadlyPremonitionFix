#pragma once

#include <string>

#include "Result.h"

namespace setup {

// Copie X3DAudio1_7.dll et DPStabilityFix.ini depuis `sourceDir` (dossier de l'installeur) vers
// `gameDir`. Une X3DAudio1_7.dll étrangère déjà présente est mise de côté (.avant-dpsf) ; un .ini
// existant est conservé (réglages de l'utilisateur).
Result InstallMod(const std::wstring& gameDir, const std::wstring& sourceDir);

}  // namespace setup
