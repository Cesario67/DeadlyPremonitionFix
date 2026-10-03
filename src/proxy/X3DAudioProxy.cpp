// Proxy de X3DAudio1_7.dll (DirectX juin 2010).
//
// DP.exe importe X3DAudio1_7.dll de façon statique, et Windows cherche d'abord les DLL dans le dossier
// de l'exécutable : en déposant notre DLL sous ce nom à côté de DP.exe, elle est chargée au démarrage
// du jeu, avant son code. Ce nom a été choisi car seul DP.exe l'importe (ni DPfix, ni PhysX, ni Steam)
// et elle n'exporte que deux fonctions, transmises telles quelles à la vraie DLL du système.

#include <windows.h>

#include <atomic>

#include "core/Log.h"

namespace {

constexpr int kExportCount = 2;
constexpr const char* kExportNames[kExportCount] = {"X3DAudioCalculate", "X3DAudioInitialize"};

std::atomic<HMODULE> g_realModule{nullptr};
std::atomic<FARPROC> g_realExports[kExportCount]{};

[[noreturn]] void FailAndExit(const wchar_t* message) noexcept {
    dpsf::log::Write(dpsf::log::Level::Error, "Vraie X3DAudio1_7.dll introuvable ou incomplète : arrêt du jeu");
    MessageBoxW(nullptr, message, L"DPStabilityFix", MB_ICONERROR | MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
    for (;;) {
    }
}

HMODULE LoadRealModule() noexcept {
    HMODULE module = g_realModule.load(std::memory_order_acquire);
    if (module != nullptr) {
        return module;
    }
    // Dans un processus 32 bits, System32 est redirigé vers SysWOW64 : on obtient la version 32 bits.
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length + 18 >= MAX_PATH) {
        FailAndExit(L"Impossible de déterminer le dossier système de Windows.");
    }
    wcscat_s(path, L"\\X3DAudio1_7.dll");
    module = LoadLibraryW(path);
    if (module == nullptr) {
        FailAndExit(
            L"La vraie X3DAudio1_7.dll est introuvable dans le dossier système.\n\n"
            L"Installez le runtime DirectX (dossier redist du jeu, DXSETUP.exe) puis relancez le jeu.");
    }
    g_realModule.store(module, std::memory_order_release);
    return module;
}

}  // namespace

// Appelé par les stubs ci-dessous : résout (une fois) l'adresse de la vraie fonction.
extern "C" FARPROC __cdecl DpsfResolveX3DAudioExport(int index) noexcept {
    FARPROC function = g_realExports[index].load(std::memory_order_acquire);
    if (function != nullptr) {
        return function;
    }
    function = GetProcAddress(LoadRealModule(), kExportNames[index]);
    if (function == nullptr) {
        FailAndExit(L"La X3DAudio1_7.dll du système n'exporte pas les fonctions attendues.");
    }
    g_realExports[index].store(function, std::memory_order_release);
    return function;
}

// Stubs « nus » : ils ne touchent pas à la pile et sautent vers la vraie fonction, qui reçoit donc
// exactement les arguments et l'adresse de retour du jeu, quelle que soit sa convention d'appel.
// EAX, ECX et EDX peuvent être écrasés : ce sont des registres libres pour l'appelant en cdecl/stdcall.
extern "C" __declspec(naked) void DpsfStubX3DAudioCalculate() {
    __asm {
        push 0
        call DpsfResolveX3DAudioExport
        add esp, 4
        jmp eax
    }
}

extern "C" __declspec(naked) void DpsfStubX3DAudioInitialize() {
    __asm {
        push 1
        call DpsfResolveX3DAudioExport
        add esp, 4
        jmp eax
    }
}
