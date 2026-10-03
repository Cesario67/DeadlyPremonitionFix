#pragma once

// Sous-ensemble de l'API de Microsoft Detours utilisé par DPfix (Detouring.cpp), réimplémenté avec
// MinHook. Detours Express 3.0, avec lequel DPfix était compilé, n'était pas libre ; cette couche évite
// de modifier le code de Durante au-delà de l'include.
//
// Sémantique conservée : les hooks attachés dans une transaction ne sont activés qu'au commit, et
// DetourAttach remplace le pointeur passé par celui de la fonction d'origine (trampoline).

#include <windows.h>

LONG DetourTransactionBegin();
LONG DetourUpdateThread(HANDLE thread);
LONG DetourAttach(PVOID* pointer, PVOID detour);
LONG DetourDetach(PVOID* pointer, PVOID detour);
LONG DetourTransactionCommit();
PVOID DetourFindFunction(LPCSTR module, LPCSTR function);
