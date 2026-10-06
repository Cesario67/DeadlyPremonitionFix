#include "graphics/RenderDiagnostics.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SystemInfo.h"
#include "graphics/ProjectionAnalysis.h"

namespace dpsf::graphics::diag {

namespace {

enum class Kind : std::uint8_t { Transform, Constants, ShaderProjection, RenderTarget, Viewport };

constexpr size_t kMaxEntries = 192;
constexpr size_t kMaxReportLines = 60;
constexpr std::uint32_t kMaxChangesLogged = 20;

struct Entry {
    Kind kind{};
    std::uintptr_t caller = 0;
    std::uint32_t a = 0;  // Transform : état | Constants, ShaderProjection : registre | RenderTarget : indice | Viewport : largeur
    std::uint32_t b = 0;  // Constants : nombre de vecteurs | RenderTarget : largeur | Viewport : hauteur
    std::uint32_t c = 0;  // RenderTarget : hauteur
    std::uint32_t d = 0;  // RenderTarget : format
    std::uint32_t calls = 0;
    std::uint32_t changes = 0;
    ProjectionInfo last{};
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::array<Entry, kMaxEntries> g_entries{};
size_t g_entryCount = 0;
bool g_overflowLogged = false;
std::uint32_t g_frames = 0;
std::int64_t g_windowStart = 0;

struct Locked {
    Locked() noexcept { AcquireSRWLockExclusive(&g_lock); }
    ~Locked() { ReleaseSRWLockExclusive(&g_lock); }
    Locked(const Locked&) = delete;
    Locked& operator=(const Locked&) = delete;
};

// Cherche (ou crée) l'entrée correspondant à la clé. Renvoie nullptr si la table est pleine.
Entry* Find(Kind kind, std::uintptr_t caller, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
            bool& created) noexcept {
    created = false;
    for (size_t i = 0; i < g_entryCount; ++i) {
        Entry& entry = g_entries[i];
        if (entry.kind == kind && entry.caller == caller && entry.a == a && entry.b == b && entry.c == c &&
            entry.d == d) {
            return &entry;
        }
    }
    if (g_entryCount >= kMaxEntries) {
        return nullptr;
    }
    Entry& entry = g_entries[g_entryCount++];
    entry.kind = kind;
    entry.caller = caller;
    entry.a = a;
    entry.b = b;
    entry.c = c;
    entry.d = d;
    created = true;
    return &entry;
}

std::string Where(std::uintptr_t caller) {
    return sysinfo::FormatAddress(caller);
}

std::string DescribeProjection(const ProjectionInfo& info, const std::string& source, std::uintptr_t caller) {
    if (info.kind == ProjectionKind::OrthoLH) {
        return std::format("Rendu : projection {}{} ({}) proche {:.3f} lointain {:.3f}, appelant {}",
                           ProjectionKindName(info.kind), info.transposed ? " transposée" : "", source,
                           info.nearPlane, info.farPlane, Where(caller));
    }
    return std::format("Rendu : projection {}{} ({}) proche {:.3f} lointain {:.3f} champ {:.1f}° format {:.3f}, "
                       "appelant {}",
                       ProjectionKindName(info.kind), info.transposed ? " transposée" : "", source, info.nearPlane,
                       info.farPlane, info.fovY, info.aspect, Where(caller));
}

std::string DescribeMatrix(const float (&m)[16]) {
    std::string text;
    for (int i = 0; i < 16; ++i) {
        text += std::format("{}{:.4g}", i == 0 ? "" : " ", m[i]);
    }
    return text;
}

// Vrai si la projection a assez changé pour mériter une nouvelle ligne de journal.
bool Differs(const ProjectionInfo& a, const ProjectionInfo& b) noexcept {
    const auto differs = [](float x, float y) { return std::fabs(x - y) > 0.005f * (std::fabs(x) + std::fabs(y)) + 1e-4f; };
    return a.kind != b.kind || differs(a.nearPlane, b.nearPlane) || differs(a.farPlane, b.farPlane);
}

// Enregistre une projection reconnue (SetTransform ou bloc de constantes) ; renvoie le message à journaliser,
// vide s'il n'y a rien de nouveau.
std::string RecordProjection(Kind kind, std::uintptr_t caller, std::uint32_t slot, const std::string& source,
                             const ProjectionInfo& info) {
    bool created = false;
    Entry* entry = Find(kind, caller, slot, 0, 0, 0, created);
    if (entry == nullptr) {
        return {};
    }
    ++entry->calls;
    if (created || (Differs(entry->last, info) && entry->changes < kMaxChangesLogged)) {
        if (!created) {
            ++entry->changes;
        }
        entry->last = info;
        return DescribeProjection(info, source, caller);
    }
    return {};
}

void Emit(const std::string& message) noexcept {
    if (message.empty()) {
        return;
    }
    try {
        log::Info("{}", message);
    } catch (...) {
    }
}

std::string LineFor(const Entry& entry, double frames) {
    const double perFrame = frames > 0.0 ? static_cast<double>(entry.calls) / frames : 0.0;
    std::string what;
    switch (entry.kind) {
        case Kind::Transform:
            what = std::format("SetTransform état {}", entry.a);
            break;
        case Kind::Constants:
            what = std::format("SetVertexShaderConstantF c{}..c{} ({} vecteurs)", entry.a, entry.a + entry.b - 1, entry.b);
            break;
        case Kind::ShaderProjection:
            what = std::format("projection dans c{}..c{} ({} {:.1f} .. {:.1f})", entry.a, entry.a + 3,
                               ProjectionKindName(entry.last.kind), entry.last.nearPlane, entry.last.farPlane);
            break;
        case Kind::RenderTarget:
            what = std::format("SetRenderTarget {} : {}x{} format {}", entry.a, entry.b, entry.c, entry.d);
            break;
        case Kind::Viewport:
            what = std::format("SetViewport {}x{}", entry.a, entry.b);
            break;
    }
    return std::format("  {} | appelant {} | {} appels ({:.1f}/image)", what, Where(entry.caller), entry.calls, perFrame);
}

}  // namespace

void OnSetTransform(const void* caller, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) noexcept {
    if (matrix == nullptr) {
        return;
    }
    std::string message;
    try {
        float m[16];
        std::memcpy(m, matrix, sizeof(m));
        const auto address = reinterpret_cast<std::uintptr_t>(caller);
        Locked lock;
        if (state == D3DTS_PROJECTION) {
            const ProjectionInfo info = AnalyzeProjection(m);
            message = RecordProjection(Kind::Transform, address, static_cast<std::uint32_t>(state), "SetTransform", info);
            if (!message.empty() && info.kind == ProjectionKind::None) {
                message = std::format("Rendu : SetTransform(projection) non reconnue, valeurs : {}, appelant {}",
                                      DescribeMatrix(m), Where(address));
            }
        } else {
            bool created = false;
            Entry* entry = Find(Kind::Transform, address, static_cast<std::uint32_t>(state), 0, 0, 0, created);
            if (entry != nullptr) {
                ++entry->calls;
            }
        }
    } catch (...) {
    }
    Emit(message);
}

void OnSetVertexShaderConstants(const void* caller, UINT startRegister, const float* data, UINT vector4Count) noexcept {
    if (data == nullptr || vector4Count < 4) {
        return;
    }
    std::vector<std::string> messages;
    try {
        const auto address = reinterpret_cast<std::uintptr_t>(caller);
        Locked lock;
        bool created = false;
        Entry* entry = Find(Kind::Constants, address, startRegister, vector4Count, 0, 0, created);
        if (entry != nullptr) {
            ++entry->calls;
            if (created) {
                float first[16];
                std::memcpy(first, data, sizeof(first));
                messages.push_back(std::format("Rendu : nouvelles constantes c{}..c{} ({} vecteurs), appelant {}, "
                                               "premières valeurs : {}",
                                               startRegister, startRegister + vector4Count - 1, vector4Count,
                                               Where(address), DescribeMatrix(first)));
            }
        }
        for (UINT block = 0; block + 4 <= vector4Count; block += 4) {
            float m[16];
            std::memcpy(m, data + block * 4, sizeof(m));
            const ProjectionInfo info = AnalyzeProjection(m);
            if (info.kind == ProjectionKind::None) {
                continue;
            }
            std::string message = RecordProjection(Kind::ShaderProjection, address, startRegister + block,
                                                   std::format("constantes c{}..c{}", startRegister + block,
                                                               startRegister + block + 3),
                                                   info);
            if (!message.empty()) {
                messages.push_back(std::move(message));
            }
        }
    } catch (...) {
    }
    for (const std::string& message : messages) {
        Emit(message);
    }
}

void OnSetRenderTarget(const void* caller, DWORD index, IDirect3DSurface9* surface) noexcept {
    std::string message;
    try {
        D3DSURFACE_DESC desc{};
        if (surface != nullptr) {
            surface->GetDesc(&desc);
        }
        const auto address = reinterpret_cast<std::uintptr_t>(caller);
        Locked lock;
        bool created = false;
        Entry* entry = Find(Kind::RenderTarget, address, index, desc.Width, desc.Height,
                            static_cast<std::uint32_t>(desc.Format), created);
        if (entry != nullptr) {
            ++entry->calls;
            if (created) {
                message = std::format("Rendu : nouvelle cible de rendu {} : {}x{} format {}, appelant {}", index,
                                      desc.Width, desc.Height, static_cast<int>(desc.Format), Where(address));
            }
        }
    } catch (...) {
    }
    Emit(message);
}

void OnSetViewport(const void* caller, const D3DVIEWPORT9* viewport) noexcept {
    if (viewport == nullptr) {
        return;
    }
    std::string message;
    try {
        const auto address = reinterpret_cast<std::uintptr_t>(caller);
        Locked lock;
        bool created = false;
        Entry* entry = Find(Kind::Viewport, address, viewport->Width, viewport->Height, 0, 0, created);
        if (entry != nullptr) {
            ++entry->calls;
            if (created) {
                message = std::format("Rendu : nouveau viewport {}x{} (profondeur {:.2f}..{:.2f}), appelant {}",
                                      viewport->Width, viewport->Height, viewport->MinZ, viewport->MaxZ, Where(address));
            }
        }
    } catch (...) {
    }
    Emit(message);
}

void OnPresent() noexcept {
    std::vector<std::string> lines;
    try {
        LARGE_INTEGER counter{}, frequency{};
        QueryPerformanceCounter(&counter);
        QueryPerformanceFrequency(&frequency);
        Locked lock;
        ++g_frames;
        if (g_windowStart == 0) {
            g_windowStart = counter.QuadPart;
            return;
        }
        const double seconds =
            static_cast<double>(counter.QuadPart - g_windowStart) / static_cast<double>(frequency.QuadPart);
        if (seconds < GetConfig().reportIntervalSeconds) {
            return;
        }
        lines.push_back(std::format("Rendu : bilan de {} images ({:.1f} s)", g_frames, seconds));
        size_t shown = 0;
        for (size_t i = 0; i < g_entryCount; ++i) {
            Entry& entry = g_entries[i];
            if (entry.calls == 0) {
                continue;
            }
            if (shown < kMaxReportLines) {
                lines.push_back(LineFor(entry, static_cast<double>(g_frames)));
                ++shown;
            }
            entry.calls = 0;
        }
        if (g_entryCount >= kMaxEntries && !g_overflowLogged) {
            g_overflowLogged = true;
            lines.push_back("  (table pleine : les nouveaux appels ne sont plus suivis)");
        }
        g_frames = 0;
        g_windowStart = counter.QuadPart;
    } catch (...) {
    }
    for (const std::string& line : lines) {
        Emit(line);
    }
}

}  // namespace dpsf::graphics::diag
