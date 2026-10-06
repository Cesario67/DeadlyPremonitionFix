#include "patches/DrawDistance.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SystemInfo.h"

namespace dpsf::patches {

namespace {

constexpr DWORD kSteam101bTimestamp = 0x529721DC;
constexpr std::uintptr_t kLoopRva = 0x2B654E;  // DP.exe 1.01b Steam, 0x6B654E
constexpr size_t kClassCount = 6;
constexpr size_t kPairSize = 12;  // fld dword ptr [adresse] (6 octets) + fstp dword ptr [ebp-disp32] (6 octets)
constexpr size_t kLoopSize = kClassCount * kPairSize;

// Adresses des constantes chargées par la boucle dans DP.exe 1.01b (lues dans .rdata : 200000, 80000,
// 20000, 5000, 1000, 500).
constexpr std::array<std::uint32_t, kClassCount> kSteamConstants = {0x00826914, 0x00779ADC, 0x00777308,
                                                                    0x00773C58, 0x00772648, 0x00772F04};

// Nos valeurs, lues par le jeu à la place des constantes (classes 1 à 5 ; l'indice 0 n'est pas utilisé).
std::array<float, kClassCount> g_distances{};

// `fld dword ptr [?]` puis `fstp dword ptr [ebp - (0xA0 - 4 * classe)]`, pour les 6 classes.
bool MatchesLoop(const unsigned char* code) noexcept {
    for (size_t index = 0; index < kClassCount; ++index) {
        const unsigned char* pair = code + index * kPairSize;
        const unsigned char displacement = static_cast<unsigned char>(0x60 + 4 * index);
        const unsigned char expected[] = {0xD9, 0x9D, displacement, 0xFF, 0xFF, 0xFF};
        if (pair[0] != 0xD9 || pair[1] != 0x05 || std::memcmp(pair + 6, expected, sizeof(expected)) != 0) {
            return false;
        }
    }
    return true;
}

std::uint32_t OperandOf(const unsigned char* code, size_t index) noexcept {
    std::uint32_t address = 0;
    std::memcpy(&address, code + index * kPairSize + 2, sizeof(address));
    return address;
}

// Boucle visée : adresse fixe dans DP.exe 1.01b, export du faux jeu sinon.
unsigned char* FindLoop(HMODULE gameModule, bool& isRealGame) {
    auto* base = reinterpret_cast<unsigned char*>(gameModule);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    isRealGame = nt->FileHeader.TimeDateStamp == kSteam101bTimestamp;
    if (isRealGame) {
        return base + kLoopRva;
    }
    auto* function = reinterpret_cast<unsigned char*>(GetProcAddress(gameModule, "DpsfTestDistanceClasses"));
    if (function == nullptr) {
        return nullptr;
    }
    if (function[0] == 0xE9) {  // liaison incrémentale : l'export pointe sur un `jmp` vers la fonction
        std::int32_t relative = 0;
        std::memcpy(&relative, function + 1, sizeof(relative));
        function += 5 + relative;
    }
    for (size_t offset = 0; offset < 64; ++offset) {
        if (MatchesLoop(function + offset)) {
            return function + offset;
        }
    }
    return nullptr;
}

}  // namespace

void ApplyDrawDistance(HMODULE gameModule) {
    const float scale = GetConfig().drawDistanceScale;
    if (scale == 1.0f) {
        return;
    }
    bool isRealGame = false;
    unsigned char* loop = FindLoop(gameModule, isRealGame);
    if (loop == nullptr) {
        return;
    }
    if (!MatchesLoop(loop)) {
        log::Warn("Distance d'affichage : code inattendu à {}, réglage ignoré",
                  sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(loop)));
        return;
    }
    if (isRealGame) {
        for (size_t index = 0; index < kClassCount; ++index) {
            if (OperandOf(loop, index) != kSteamConstants[index]) {
                log::Warn("Distance d'affichage : constante inattendue pour la classe {}, réglage ignoré", index);
                return;
            }
        }
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(loop, kLoopSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::Warn("Distance d'affichage : VirtualProtect a échoué ({})", GetLastError());
        return;
    }
    std::string changes;
    for (size_t index = 1; index < kClassCount; ++index) {
        const float original = *reinterpret_cast<const float*>(static_cast<std::uintptr_t>(OperandOf(loop, index)));
        g_distances[index] = original * scale;
        const auto address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_distances[index]));
        std::memcpy(loop + index * kPairSize + 2, &address, sizeof(address));
        changes += std::format("{}{:.0f} -> {:.0f}", index == 1 ? "" : ", ", original, g_distances[index]);
    }
    VirtualProtect(loop, kLoopSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), loop, kLoopSize);
    log::Info("Distance d'affichage x{:.2f} (classes 1 à 5 de la caméra, {}) : {}", scale,
              sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(loop)), changes);
}

}  // namespace dpsf::patches
