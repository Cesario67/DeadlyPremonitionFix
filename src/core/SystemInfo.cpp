#include "core/SystemInfo.h"

#include <psapi.h>

#include <cmath>
#include <format>

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"

namespace dpsf::sysinfo {

namespace {

constexpr std::uint64_t kMegabyte = 1024ull * 1024ull;

using NtQueryTimerResolutionFn = LONG(NTAPI*)(PULONG minimum, PULONG maximum, PULONG current);
using RtlGetVersionFn = LONG(NTAPI*)(PRTL_OSVERSIONINFOW info);

template <typename Fn>
Fn NtdllFunction(const char* name) noexcept {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll != nullptr ? reinterpret_cast<Fn>(GetProcAddress(ntdll, name)) : nullptr;
}

}  // namespace

MemorySnapshot QueryMemory() noexcept {
    MemorySnapshot snapshot;
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        snapshot.addressSpaceTotalMb = status.ullTotalVirtual / kMegabyte;
        snapshot.addressSpaceUsedMb = (status.ullTotalVirtual - status.ullAvailVirtual) / kMegabyte;
        snapshot.physicalAvailableMb = status.ullAvailPhys / kMegabyte;
    }
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                sizeof(counters))) {
        snapshot.privateMb = counters.PrivateUsage / kMegabyte;
    }
    return snapshot;
}

double QueryTimerResolutionMs() noexcept {
    static const auto query = NtdllFunction<NtQueryTimerResolutionFn>("NtQueryTimerResolution");
    if (query == nullptr) {
        return 0.0;
    }
    ULONG minimum = 0;
    ULONG maximum = 0;
    ULONG current = 0;
    if (query(&minimum, &maximum, &current) != 0) {
        return 0.0;
    }
    return static_cast<double>(current) / 10000.0;  // unités de 100 ns
}

int QueryX87PrecisionBits() noexcept {
    unsigned short control = 0;
    __asm fnstcw control
    switch ((control >> 8) & 3) {
        case 0:
            return 24;
        case 2:
            return 53;
        case 3:
            return 64;
        default:
            return 0;
    }
}

double GameTimeResolutionMs(int mantissaBits) noexcept {
    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    const double microseconds = static_cast<double>(counter.QuadPart) * 1e6 / static_cast<double>(frequency.QuadPart);
    if (microseconds <= 0.0 || mantissaBits <= 0) {
        return 0.0;
    }
    // Écart entre deux valeurs représentables consécutives autour de `microseconds`.
    int exponent = 0;
    std::frexp(microseconds, &exponent);
    return std::ldexp(1.0, exponent - mantissaBits) / 1000.0;
}

std::wstring ModulePathOf(const void* address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(address), &module)) {
        return {};
    }
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    return path;
}

std::string FormatAddress(std::uintptr_t address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module)) {
        return std::format("0x{:08X}", address);
    }
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring_view name(path);
    const size_t separator = name.find_last_of(L"\\/");
    if (separator != std::wstring_view::npos) {
        name.remove_prefix(separator + 1);
    }
    return std::format("{}+0x{:X} (0x{:08X})", WideToUtf8(name), address - reinterpret_cast<std::uintptr_t>(module),
                       address);
}

void LogStartupInfo(HMODULE gameModule) {
    const Paths& paths = GetPaths();
    log::Info("Exécutable : {}", WideToUtf8(paths.gameExe));

    const auto* base = reinterpret_cast<const BYTE*>(gameModule);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const bool largeAddressAware = (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
    log::Info("DP.exe : horodatage PE 0x{:08X}, taille image 0x{:X}, LARGE_ADDRESS_AWARE={}",
              nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, largeAddressAware ? "oui" : "non");
    // 0x529721DC (28/11/2013) : version Steam 1.01b analysée pour ce mod.
    if (nt->FileHeader.TimeDateStamp != 0x529721DCu) {
        log::Warn("Version de DP.exe différente de celle analysée (1.01b Steam, 0x529721DC)");
    }

    if (const auto getVersion = NtdllFunction<RtlGetVersionFn>("RtlGetVersion")) {
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (getVersion(&version) == 0) {
            log::Info("Windows {}.{} build {}", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
        }
    }

    BOOL isWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &isWow64);
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    log::Info("Processeurs logiques : {} | RAM : {} Mo | WOW64 : {}", GetActiveProcessorCount(ALL_PROCESSOR_GROUPS),
              status.ullTotalPhys / kMegabyte, isWow64 ? "oui" : "non");

    const MemorySnapshot memory = QueryMemory();
    log::Info("Espace d'adressage du processus : {} Mo au total", memory.addressSpaceTotalMb);
    // Depuis Windows 10 2004, cette valeur est globale : un autre programme peut l'abaisser sans que
    // les Sleep du jeu en profitent (la résolution est désormais gérée par processus).
    log::Info("Résolution du minuteur Windows (globale) au démarrage : {:.3f} ms", QueryTimerResolutionMs());

    // Le démarrage rapide de Windows ne remet pas ce compteur à zéro : seul un vrai redémarrage le fait.
    LARGE_INTEGER counter{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    const double hours = static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart) / 3600.0;
    log::Info("Compteur haute précision : {} Hz, {:.1f} h depuis le démarrage | résolution du temps du jeu : "
              "{:.3f} ms en simple précision, {:.4f} µs en double",
              frequency.QuadPart, hours, GameTimeResolutionMs(24), GameTimeResolutionMs(53) * 1000.0);
}

}  // namespace dpsf::sysinfo
