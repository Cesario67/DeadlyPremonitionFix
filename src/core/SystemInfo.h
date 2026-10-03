#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

namespace dpsf::sysinfo {

struct MemorySnapshot {
    std::uint64_t addressSpaceUsedMb = 0;   // espace d'adressage virtuel utilisé par le processus
    std::uint64_t addressSpaceTotalMb = 0;  // 2048 sans LARGE_ADDRESS_AWARE, ~4096 avec
    std::uint64_t privateMb = 0;            // mémoire privée allouée
    std::uint64_t physicalAvailableMb = 0;
};

MemorySnapshot QueryMemory() noexcept;

// Résolution actuelle du minuteur Windows, en millisecondes (0 si indisponible).
double QueryTimerResolutionMs() noexcept;

// « DP.exe+0x1A2B3C » ou l'adresse brute si elle n'appartient à aucun module.
std::string FormatAddress(std::uintptr_t address);

// Chemin du module qui contient `address` (vide si aucun).
std::wstring ModulePathOf(const void* address);

// Journalise la configuration de la machine et de l'exécutable du jeu.
void LogStartupInfo(HMODULE gameModule);

}  // namespace dpsf::sysinfo
