#include "patches/ZeroDeltaGuard.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SystemInfo.h"

namespace dpsf::patches {

namespace {

constexpr DWORD kSteam101bTimestamp = 0x529721DC;
constexpr std::uintptr_t kSiteRva = 0x18CB09;         // DP.exe 1.01b Steam, 0x58CB09
constexpr std::uintptr_t kFrameDeltaRva = 0x10AFFE0;  // frameDelta : 0x14AFFE0

// Instructions remplacées (16 octets, identifiées par ZachFix, vérifiées sur DP.exe 1.01b) :
//   fld  dword ptr [esp+10h]        ; déplacement horizontal
//   fdiv dword ptr [frameDelta]     ; / frameDelta
//   fstp dword ptr [esi+4E4h]       ; vitesse
// Les 4 octets d'adresse de frameDelta (décalage 6) sont laissés libres dans le motif.
constexpr std::array<unsigned char, 16> kExpected = {0xD9, 0x44, 0x24, 0x10, 0xD8, 0x35, 0x00, 0x00,
                                                     0x00, 0x00, 0xD9, 0x9E, 0xE4, 0x04, 0x00, 0x00};
constexpr size_t kAddressOffset = 6;
constexpr size_t kAddressSize = 4;

volatile LONG g_hits = 0;

bool MatchesPattern(const unsigned char* code) noexcept {
    for (size_t i = 0; i < kExpected.size(); ++i) {
        if (i >= kAddressOffset && i < kAddressOffset + kAddressSize) {
            continue;
        }
        if (code[i] != kExpected[i]) {
            return false;
        }
    }
    return true;
}

// Petit assembleur x86 : ajoute des octets au relais.
class Emitter {
public:
    explicit Emitter(unsigned char* start) noexcept : cursor_(start), start_(start) {}
    void Byte(unsigned char value) noexcept { *cursor_++ = value; }
    void Dword(std::uint32_t value) noexcept {
        std::memcpy(cursor_, &value, sizeof(value));
        cursor_ += sizeof(value);
    }
    void Jump(const void* destination) noexcept {  // jmp rel32
        Byte(0xE9);
        Dword(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(destination) -
                                         (reinterpret_cast<std::uintptr_t>(cursor_) + 4)));
    }
    unsigned char* Position() const noexcept { return cursor_; }
    size_t Size() const noexcept { return static_cast<size_t>(cursor_ - start_); }

private:
    unsigned char* cursor_;
    unsigned char* start_;
};

// Relais exécuté à la place des 16 octets. Il ne touche qu'à eax et aux indicateurs (sauvegardés), jamais
// à la pile de calcul x87 au-delà de ce que faisait l'original.
//   pushfd / push eax                 ; les deux pushs décalent [esp+10h] en [esp+18h]
//   mov eax, [frameDelta] ; and eax, 7FFFFFFFh      ; |bits| == 0 : +0 ou -0
//   jne normal
//   fld [esp+18h] ; fstp [esi+4E4h]   ; delta 0 -> déplacement / 1.0 = déplacement
//   lock inc [g_hits] ; pop eax ; popfd ; jmp retour
// normal:
//   fld [esp+18h] ; fdiv [frameDelta] ; fstp [esi+4E4h] ; pop eax ; popfd ; jmp retour
bool BuildStub(unsigned char* stub, std::uintptr_t frameDelta, const void* returnAddress) noexcept {
    Emitter code(stub);
    code.Byte(0x9C);  // pushfd
    code.Byte(0x50);  // push eax
    code.Byte(0xA1);  // mov eax, [frameDelta]
    code.Dword(static_cast<std::uint32_t>(frameDelta));
    code.Byte(0x25);  // and eax, 0x7FFFFFFF
    code.Dword(0x7FFFFFFF);
    code.Byte(0x75);  // jne normal
    unsigned char* displacement = code.Position();
    code.Byte(0x00);

    code.Byte(0xD9);  // fld dword ptr [esp+18h]
    code.Byte(0x44);
    code.Byte(0x24);
    code.Byte(0x18);
    code.Byte(0xD9);  // fstp dword ptr [esi+4E4h]
    code.Byte(0x9E);
    code.Dword(0x4E4);
    code.Byte(0xF0);  // lock inc dword ptr [g_hits]
    code.Byte(0xFF);
    code.Byte(0x05);
    code.Dword(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_hits)));
    code.Byte(0x58);  // pop eax
    code.Byte(0x9D);  // popfd
    code.Jump(returnAddress);

    unsigned char* normal = code.Position();
    code.Byte(0xD9);  // fld dword ptr [esp+18h]
    code.Byte(0x44);
    code.Byte(0x24);
    code.Byte(0x18);
    code.Byte(0xD8);  // fdiv dword ptr [frameDelta]
    code.Byte(0x35);
    code.Dword(static_cast<std::uint32_t>(frameDelta));
    code.Byte(0xD9);  // fstp dword ptr [esi+4E4h]
    code.Byte(0x9E);
    code.Dword(0x4E4);
    code.Byte(0x58);  // pop eax
    code.Byte(0x9D);  // popfd
    code.Jump(returnAddress);

    const std::ptrdiff_t jump = normal - (displacement + 1);
    if (jump < 0 || jump > 127) {
        return false;
    }
    *displacement = static_cast<unsigned char>(jump);
    return true;
}

// Trouve les 16 octets visés : adresse fixe dans DP.exe 1.01b, export du faux jeu sinon.
unsigned char* FindSite(HMODULE gameModule, bool& isRealGame) {
    auto* base = reinterpret_cast<unsigned char*>(gameModule);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    isRealGame = nt->FileHeader.TimeDateStamp == kSteam101bTimestamp;
    if (isRealGame) {
        return base + kSiteRva;
    }
    // Faux jeu des tests : fonction exportée qui contient la même séquence.
    auto* function = reinterpret_cast<unsigned char*>(GetProcAddress(gameModule, "DpsfTestSpeedDivide"));
    if (function == nullptr) {
        return nullptr;
    }
    if (function[0] == 0xE9) {  // liaison incrémentale : l'export pointe sur un `jmp` vers la fonction
        std::int32_t relative = 0;
        std::memcpy(&relative, function + 1, sizeof(relative));
        function += 5 + relative;
    }
    for (size_t offset = 0; offset < 96; ++offset) {
        if (MatchesPattern(function + offset)) {
            return function + offset;
        }
    }
    return nullptr;
}

}  // namespace

void ApplyZeroDeltaGuard(HMODULE gameModule) {
    if (!GetConfig().zeroDeltaGuard) {
        return;
    }
    bool isRealGame = false;
    unsigned char* site = FindSite(gameModule, isRealGame);
    if (site == nullptr) {
        return;
    }
    if (!MatchesPattern(site)) {
        log::Warn("Delta nul : code inattendu à {}, correctif ignoré", sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(site)));
        return;
    }
    std::uint32_t frameDelta = 0;
    std::memcpy(&frameDelta, site + kAddressOffset, sizeof(frameDelta));
    if (isRealGame && frameDelta != reinterpret_cast<std::uintptr_t>(gameModule) + kFrameDeltaRva) {
        log::Warn("Delta nul : adresse de frameDelta inattendue (0x{:08X}), correctif ignoré", frameDelta);
        return;
    }

    constexpr size_t kStubCapacity = 128;
    void* memory = VirtualAlloc(nullptr, kStubCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (memory == nullptr) {
        log::Warn("Delta nul : VirtualAlloc a échoué ({})", GetLastError());
        return;
    }
    auto* stub = static_cast<unsigned char*>(memory);
    if (!BuildStub(stub, frameDelta, site + kExpected.size())) {
        VirtualFree(memory, 0, MEM_RELEASE);
        log::Warn("Delta nul : relais non construit, correctif ignoré");
        return;
    }
    DWORD ignored = 0;
    VirtualProtect(memory, kStubCapacity, PAGE_EXECUTE_READ, &ignored);
    FlushInstructionCache(GetCurrentProcess(), memory, kStubCapacity);

    // Remplace les 16 octets par `jmp relais` complété de nop.
    DWORD oldProtect = 0;
    if (!VirtualProtect(site, kExpected.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::Warn("Delta nul : VirtualProtect a échoué ({})", GetLastError());
        return;
    }
    std::memset(site, 0x90, kExpected.size());
    site[0] = 0xE9;
    const auto relative = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stub) -
                                                     (reinterpret_cast<std::uintptr_t>(site) + 5));
    std::memcpy(site + 1, &relative, sizeof(relative));
    VirtualProtect(site, kExpected.size(), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), site, kExpected.size());
    log::Info("Delta nul : calcul de vitesse protégé à {} (frameDelta = 0 : un tick de 60 Hz)",
              sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(site)));
}

long ZeroDeltaGuardHits() noexcept {
    return InterlockedCompareExchange(&g_hits, 0, 0);
}

}  // namespace dpsf::patches
