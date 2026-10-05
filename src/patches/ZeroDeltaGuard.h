#pragma once

#include <windows.h>

namespace dpsf::patches {

// Évite la division par zéro du calcul de vitesse de déplacement de DP.exe. À 0x58CB09 (1.01b Steam), le jeu
// fait `vitesse = déplacement / frameDelta`, où frameDelta vaut `secondes écoulées * 60`. Quand le delta est
// exactement 0, le résultat est NaN (0/0) ou INF (fini/0, qui devient NaN plus loin) et le jeu s'enferme dans
// sa boucle volontaire contre les flottants invalides : il se fige.
// Le bug et la forme du correctif viennent de ZachFix (h714je, GPL-3.0, « zero-delta speed fix ») ; code
// réécrit ici. Delta nul : on utilise 1,0 (un tick de 60 Hz), donc vitesse = déplacement ; sinon l'instruction
// d'origine s'exécute telle quelle. Le code cible est réécrit en mémoire (saut vers un petit relais), après
// vérification des octets : sûr dans DllMain, avant que le jeu ne l'exécute.
void ApplyZeroDeltaGuard(HMODULE gameModule);

// Nombre de fois où le relais a remplacé un delta nul depuis le lancement (0 si le correctif est inactif).
long ZeroDeltaGuardHits() noexcept;

}  // namespace dpsf::patches
