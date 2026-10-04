#include "patches/SkipIntro.h"

#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SystemInfo.h"

namespace dpsf::patches {

namespace {

constexpr DWORD kSteam101bTimestamp = 0x529721DC;
constexpr std::uintptr_t kInstructionRva = 0x243F2D;  // DP.exe 1.01b Steam, 0x643F2D
// mov dword ptr [0x014736D8], 0x000000B3
constexpr unsigned char kExpected[] = {0xC7, 0x05, 0xD8, 0x36, 0x47, 0x01, 0xB3, 0x00, 0x00, 0x00};
constexpr size_t kImmediateOffset = 6;

}  // namespace

void ApplySkipIntro(HMODULE gameModule) {
    if (!GetConfig().skipIntro) {
        return;
    }
    const auto* base = reinterpret_cast<unsigned char*>(gameModule);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    unsigned char* instruction = nullptr;
    if (nt->FileHeader.TimeDateStamp == kSteam101bTimestamp) {
        instruction = const_cast<unsigned char*>(base) + kInstructionRva;
    } else {
        // Faux jeu des tests : copie de l'instruction exportée (DP.exe n'exporte rien).
        instruction = reinterpret_cast<unsigned char*>(GetProcAddress(gameModule, "DpsfTestIntroInstruction"));
        if (instruction == nullptr) {
            return;
        }
    }
    if (instruction[kImmediateOffset] == 0x00 && std::memcmp(instruction, kExpected, kImmediateOffset) == 0) {
        log::Info("Logos et introduction : déjà sautés (DP.exe modifié à la main)");
        return;
    }
    if (std::memcmp(instruction, kExpected, sizeof(kExpected)) != 0) {
        log::Warn("Logos et introduction : code inattendu à {}, non sautés",
                  sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(instruction)));
        return;
    }
    DWORD oldProtect = 0;
    if (!VirtualProtect(instruction + kImmediateOffset, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::Warn("Logos et introduction : VirtualProtect a échoué ({})", GetLastError());
        return;
    }
    instruction[kImmediateOffset] = 0x00;
    VirtualProtect(instruction + kImmediateOffset, 1, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), instruction, sizeof(kExpected));
    log::Info("Logos et introduction sautés au lancement (étape de départ 0xB3 -> 0 à {})",
              sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(instruction)));
}

}  // namespace dpsf::patches
