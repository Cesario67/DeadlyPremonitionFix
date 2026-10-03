#pragma once

#include <windows.h>

namespace dpsf::frames {

// Intercepte Sleep dans DP.exe pour mesurer la précision réelle de ses attentes.
void Install(HMODULE gameModule);

// Applique la résolution de minuteur demandée (TimerResolutionMs). Appelé hors de DllMain.
void LateInit() noexcept;

// Encadrent chaque IDirect3DDevice9::Present. OnAfterPresent renvoie true quand un rapport
// périodique vient d'être écrit dans le journal.
void OnBeforePresent() noexcept;
bool OnAfterPresent() noexcept;

// Après un Reset du périphérique (alt-tab, changement de mode) : ne pas compter l'interruption
// comme une saccade.
void OnDeviceReset() noexcept;

void LogFinalReport() noexcept;

}  // namespace dpsf::frames
