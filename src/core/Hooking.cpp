#include "core/Hooking.h"

#include <cstring>

namespace dpsf::hooking {

bool PatchPointer(void** slot, void* replacement, void** original) noexcept {
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    void* previous = InterlockedExchangePointer(slot, replacement);
    VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
    if (original != nullptr) {
        *original = previous;
    }
    return true;
}

bool PatchImportRaw(HMODULE module, const char* dllName, const char* functionName, void* replacement,
                    void** original) noexcept {
    auto* base = reinterpret_cast<BYTE*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0) {
        return false;
    }

    for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
         descriptor->Name != 0; ++descriptor) {
        if (_stricmp(reinterpret_cast<const char*>(base + descriptor->Name), dllName) != 0) {
            continue;
        }
        // Sans OriginalFirstThunk, impossible de retrouver les noms : DP.exe en a bien un.
        if (descriptor->OriginalFirstThunk == 0) {
            continue;
        }
        auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
        auto* addressThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; nameThunk->u1.AddressOfData != 0; ++nameThunk, ++addressThunk) {
            if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal)) {
                continue;
            }
            const auto* importByName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(importByName->Name), functionName) == 0) {
                return PatchPointer(reinterpret_cast<void**>(&addressThunk->u1.Function), replacement, original);
            }
        }
    }
    return false;
}

}  // namespace dpsf::hooking
