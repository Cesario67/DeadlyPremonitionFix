#pragma once

#include <windows.h>

namespace dpsf::hooking {

// Remplace un pointeur en mémoire protégée (entrée d'IAT, case de vtable) et renvoie l'ancienne valeur.
bool PatchPointer(void** slot, void* replacement, void** original) noexcept;

// Redirige un import de `module` (ex. "kernel32.dll" / "CreateFileA") vers `replacement`.
// Seuls les appels faits par ce module sont interceptés : les autres DLL (DPfix, Steam...) ne sont
// pas concernées.
bool PatchImportRaw(HMODULE module, const char* dllName, const char* functionName, void* replacement,
                    void** original) noexcept;

template <typename Fn>
bool PatchImport(HMODULE module, const char* dllName, const char* functionName, Fn replacement,
                 Fn* original) noexcept {
    void* previous = nullptr;
    if (!PatchImportRaw(module, dllName, functionName, reinterpret_cast<void*>(replacement), &previous)) {
        return false;
    }
    *original = reinterpret_cast<Fn>(previous);
    return true;
}

// Remplace l'entrée `index` de la vtable d'un objet COM. Renvoie false si la case pointe déjà
// vers `replacement`.
template <typename Fn>
bool PatchVtable(void* object, size_t index, Fn replacement, Fn* original) noexcept {
    void** vtable = *reinterpret_cast<void***>(object);
    if (vtable[index] == reinterpret_cast<void*>(replacement)) {
        return false;
    }
    void* previous = nullptr;
    if (!PatchPointer(&vtable[index], reinterpret_cast<void*>(replacement), &previous)) {
        return false;
    }
    *original = reinterpret_cast<Fn>(previous);
    return true;
}

}  // namespace dpsf::hooking
