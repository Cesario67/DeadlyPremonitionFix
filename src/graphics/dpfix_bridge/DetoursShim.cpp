#include "DetoursShim.h"

#include <MinHook.h>

#include <vector>

namespace {

struct Hook {
    PVOID target;      // fonction interceptée
    PVOID trampoline;  // appel de la fonction d'origine
};

std::vector<Hook> g_hooks;
bool g_initialized = false;

bool EnsureInitialized() {
    if (!g_initialized) {
        const MH_STATUS status = MH_Initialize();
        g_initialized = status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
    }
    return g_initialized;
}

}  // namespace

LONG DetourTransactionBegin() {
    return EnsureInitialized() ? NO_ERROR : ERROR_INVALID_OPERATION;
}

LONG DetourUpdateThread(HANDLE) {
    // MinHook suspend lui-même les autres threads pendant la modification du code.
    return NO_ERROR;
}

LONG DetourAttach(PVOID* pointer, PVOID detour) {
    if (pointer == nullptr || *pointer == nullptr || !EnsureInitialized()) {
        return ERROR_INVALID_PARAMETER;
    }
    PVOID target = *pointer;
    PVOID trampoline = nullptr;
    if (MH_CreateHook(target, detour, &trampoline) != MH_OK || MH_QueueEnableHook(target) != MH_OK) {
        return ERROR_INVALID_OPERATION;
    }
    g_hooks.push_back({target, trampoline});
    *pointer = trampoline;
    return NO_ERROR;
}

LONG DetourDetach(PVOID* pointer, PVOID) {
    if (pointer == nullptr) {
        return ERROR_INVALID_PARAMETER;
    }
    for (auto it = g_hooks.begin(); it != g_hooks.end(); ++it) {
        if (it->trampoline == *pointer) {
            MH_QueueDisableHook(it->target);
            *pointer = it->target;
            g_hooks.erase(it);
            return NO_ERROR;
        }
    }
    return ERROR_INVALID_HANDLE;
}

LONG DetourTransactionCommit() {
    return MH_ApplyQueued() == MH_OK ? NO_ERROR : ERROR_INVALID_OPERATION;
}

PVOID DetourFindFunction(LPCSTR module, LPCSTR function) {
    HMODULE handle = GetModuleHandleA(module);
    if (handle == nullptr) {
        handle = LoadLibraryA(module);
    }
    return handle != nullptr ? reinterpret_cast<PVOID>(GetProcAddress(handle, function)) : nullptr;
}
