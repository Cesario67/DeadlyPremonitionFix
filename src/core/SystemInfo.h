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

// Précision du x87 du thread appelant, en bits de mantisse (24, 53 ou 64).
int QueryX87PrecisionBits() noexcept;

// Résolution, en millisecondes, du temps que calcule DP.exe à partir de la valeur absolue de
// QueryPerformanceCounter (0x401F50 : microsecondes depuis le démarrage du PC) avec `mantissaBits`
// bits de précision. Elle se dégrade avec la durée écoulée depuis le démarrage.
double GameTimeResolutionMs(int mantissaBits) noexcept;

// « DP.exe+0x1A2B3C » ou l'adresse brute si elle n'appartient à aucun module.
std::string FormatAddress(std::uintptr_t address);

// Chemin du module qui contient `address` (vide si aucun).
std::wstring ModulePathOf(const void* address);

// Journalise la configuration de la machine et de l'exécutable du jeu.
void LogStartupInfo(HMODULE gameModule);

}  // namespace dpsf::sysinfo
