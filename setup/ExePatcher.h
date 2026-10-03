#pragma once

#include <windows.h>

#include <string>

#include "Result.h"

namespace setup {

// Version Steam 1.01b de DP.exe, celle analysée pour le mod.
constexpr DWORD kKnownGameTimestamp = 0x529721DC;

struct ExeInfo {
    bool valid = false;  // en-tête PE lisible
    bool x86 = false;
    DWORD timestamp = 0;
    bool largeAddressAware = false;
};

ExeInfo ReadExeInfo(const std::wstring& exePath);

// Vrai si le fichier est ouvert par un autre processus (le jeu tourne).
bool IsFileInUse(const std::wstring& path);

// Patch « 4 Go » : active LARGE_ADDRESS_AWARE dans l'en-tête de DP.exe, pour que le jeu (32 bits)
// dispose de ~4 Go d'espace d'adressage au lieu de 2. Une copie de l'original (DP.exe.dpsf-original)
// est faite avant la première modification, et le fichier est remplacé de façon atomique.
Result EnableLargeAddressAware(const std::wstring& exePath);

}  // namespace setup
