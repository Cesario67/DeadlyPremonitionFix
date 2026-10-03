#include "framepacing/FrameMonitor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/SystemInfo.h"

namespace dpsf::frames {

namespace {

using SleepFn = void(WINAPI*)(DWORD);
using TimeBeginPeriodFn = UINT(WINAPI*)(UINT);

constexpr size_t kMaxSamples = 8192;
constexpr double kHitchThresholdMs = 50.0;  // 1,5 image à 30 i/s

SleepFn g_sleep = nullptr;
LARGE_INTEGER g_frequency{};

// Données du thread de rendu : écrites uniquement depuis Present, donc sans verrou.
std::array<float, kMaxSamples> g_samples{};
size_t g_sampleCount = 0;
std::int64_t g_lastPresent = 0;
std::int64_t g_windowStart = 0;
std::uint64_t g_totalFrames = 0;
std::uint64_t g_totalHitches = 0;
std::atomic<DWORD> g_renderThreadId{0};

// Limiteur optionnel.
std::int64_t g_limitPeriod = 0;
std::int64_t g_nextDeadline = 0;
HANDLE g_waitTimer = nullptr;

// Statistiques de Sleep, alimentées par tous les threads du jeu.
struct SleepStats {
    std::atomic<std::uint32_t> calls{0};
    std::atomic<std::uint32_t> zeroCalls{0};
    std::atomic<std::uint64_t> requestedMs{0};
    std::atomic<std::uint64_t> actualUs{0};
    std::atomic<std::uint32_t> maxOvershootUs{0};
};
SleepStats g_renderSleep;
SleepStats g_otherSleep;

std::int64_t Now() noexcept {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

double ToMs(std::int64_t ticks) noexcept {
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(g_frequency.QuadPart);
}

void WINAPI HookSleep(DWORD milliseconds) {
    SleepStats& stats = GetCurrentThreadId() == g_renderThreadId.load(std::memory_order_relaxed) ? g_renderSleep
                                                                                                   : g_otherSleep;
    if (milliseconds == 0) {
        stats.zeroCalls.fetch_add(1, std::memory_order_relaxed);
        g_sleep(milliseconds);
        return;
    }
    const std::int64_t start = Now();
    g_sleep(milliseconds);
    const auto elapsedUs = static_cast<std::uint64_t>(ToMs(Now() - start) * 1000.0);

    stats.calls.fetch_add(1, std::memory_order_relaxed);
    stats.requestedMs.fetch_add(milliseconds, std::memory_order_relaxed);
    stats.actualUs.fetch_add(elapsedUs, std::memory_order_relaxed);
    const std::uint64_t requestedUs = static_cast<std::uint64_t>(milliseconds) * 1000;
    const auto overshoot = static_cast<std::uint32_t>(elapsedUs > requestedUs ? elapsedUs - requestedUs : 0);
    std::uint32_t currentMax = stats.maxOvershootUs.load(std::memory_order_relaxed);
    while (overshoot > currentMax &&
           !stats.maxOvershootUs.compare_exchange_weak(currentMax, overshoot, std::memory_order_relaxed)) {
    }
}

std::string DescribeSleep(SleepStats& stats) {
    const std::uint32_t calls = stats.calls.exchange(0, std::memory_order_relaxed);
    const std::uint32_t zeroCalls = stats.zeroCalls.exchange(0, std::memory_order_relaxed);
    const std::uint64_t requestedMs = stats.requestedMs.exchange(0, std::memory_order_relaxed);
    const std::uint64_t actualUs = stats.actualUs.exchange(0, std::memory_order_relaxed);
    const std::uint32_t maxOvershootUs = stats.maxOvershootUs.exchange(0, std::memory_order_relaxed);
    if (calls == 0) {
        return std::format("aucun (Sleep(0) : {})", zeroCalls);
    }
    return std::format("{} appels, demandé {:.2f} ms moy, réel {:.2f} ms moy, dépassement max {:.2f} ms (Sleep(0) : {})",
                       calls, static_cast<double>(requestedMs) / calls, static_cast<double>(actualUs) / 1000.0 / calls,
                       maxOvershootUs / 1000.0, zeroCalls);
}

// Attente précise : minuteur haute résolution pour l'essentiel, puis attente active sur la fin.
void WaitUntil(std::int64_t deadline) noexcept {
    const double remainingMs = ToMs(deadline - Now());
    if (remainingMs > 2.0 && g_waitTimer != nullptr) {
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>((remainingMs - 1.5) * 10000.0);  // relatif, unités de 100 ns
        if (SetWaitableTimerEx(g_waitTimer, &due, 0, nullptr, nullptr, nullptr, 0)) {
            WaitForSingleObject(g_waitTimer, INFINITE);
        }
    }
    while (Now() < deadline) {
        YieldProcessor();
    }
}

void Report(std::int64_t now) {
    const double windowSeconds = ToMs(now - g_windowStart) / 1000.0;
    if (g_sampleCount > 0) {
        float* begin = g_samples.data();
        float* end = begin + g_sampleCount;
        double sum = 0.0;
        size_t hitches = 0;
        for (const float* it = begin; it != end; ++it) {
            sum += *it;
            if (*it > kHitchThresholdMs) {
                ++hitches;
            }
        }
        const auto percentile = [&](double fraction) {
            float* nth = begin + static_cast<size_t>(fraction * static_cast<double>(g_sampleCount - 1));
            std::nth_element(begin, nth, end);
            return *nth;
        };
        const float median = percentile(0.50);
        const float p99 = percentile(0.99);
        const float maximum = *std::max_element(begin, end);
        g_totalHitches += hitches;
        log::Info("Images : {:.1f} i/s | moy {:.1f} ms | méd {:.1f} | p99 {:.1f} | max {:.1f} | >{:.0f} ms : {}",
                  static_cast<double>(g_sampleCount) / windowSeconds, sum / static_cast<double>(g_sampleCount), median,
                  p99, maximum, kHitchThresholdMs, hitches);
    }
    log::Info("  Sleep thread de rendu : {}", DescribeSleep(g_renderSleep));
    log::Info("  Sleep autres threads : {}", DescribeSleep(g_otherSleep));
    const sysinfo::MemorySnapshot memory = sysinfo::QueryMemory();
    log::Info("  Mémoire : espace d'adressage {}/{} Mo, privée {} Mo | minuteur {:.3f} ms", memory.addressSpaceUsedMb,
              memory.addressSpaceTotalMb, memory.privateMb, sysinfo::QueryTimerResolutionMs());
}

}  // namespace

void Install(HMODULE gameModule) {
    QueryPerformanceFrequency(&g_frequency);
    const Config& config = GetConfig();
    if (!config.frameStats) {
        log::Info("Mesures de cadence désactivées (FrameStats=0)");
    } else if (!hooking::PatchImport(gameModule, "kernel32.dll", "Sleep", &HookSleep, &g_sleep)) {
        log::Warn("Interception de Sleep impossible : pas de statistiques d'attente");
    }
    if (config.frameLimitFps > 0) {
        g_limitPeriod = g_frequency.QuadPart / config.frameLimitFps;
    }
}

void LateInit() noexcept {
    const Config& config = GetConfig();
    if (config.timerResolutionMs > 0) {
        const double before = sysinfo::QueryTimerResolutionMs();
        const HMODULE winmm = LoadLibraryW(L"winmm.dll");
        const auto timeBeginPeriod =
            winmm != nullptr ? reinterpret_cast<TimeBeginPeriodFn>(GetProcAddress(winmm, "timeBeginPeriod")) : nullptr;
        if (timeBeginPeriod != nullptr && timeBeginPeriod(static_cast<UINT>(config.timerResolutionMs)) == 0) {
            log::Info("Résolution du minuteur : {:.3f} ms -> {:.3f} ms (timeBeginPeriod({}))", before,
                      sysinfo::QueryTimerResolutionMs(), config.timerResolutionMs);
        } else {
            log::Warn("timeBeginPeriod({}) a échoué", config.timerResolutionMs);
        }
    } else {
        log::Info("Résolution du minuteur laissée telle quelle (TimerResolutionMs=0) : {:.3f} ms",
                  sysinfo::QueryTimerResolutionMs());
    }

    if (g_limitPeriod > 0) {
        g_waitTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (g_waitTimer == nullptr) {
            g_waitTimer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        }
        log::Info("Limiteur d'images actif : {} i/s", config.frameLimitFps);
    }
}

void OnBeforePresent() noexcept {
    if (g_limitPeriod == 0) {
        return;
    }
    if (g_nextDeadline != 0) {
        WaitUntil(g_nextDeadline);
    }
    const std::int64_t now = Now();
    // En retard de plus d'une image : on repart de maintenant au lieu d'enchaîner des images trop rapides.
    g_nextDeadline = (g_nextDeadline == 0 || now - g_nextDeadline > g_limitPeriod) ? now + g_limitPeriod
                                                                                    : g_nextDeadline + g_limitPeriod;
}

bool OnAfterPresent() noexcept {
    const std::int64_t now = Now();
    ++g_totalFrames;
    if (g_lastPresent == 0) {
        if (g_renderThreadId.exchange(GetCurrentThreadId(), std::memory_order_relaxed) == 0) {
            log::Info("Première image présentée (thread de rendu {})", GetCurrentThreadId());
        }
        g_lastPresent = now;
        g_windowStart = now;
        return false;
    }
    if (!GetConfig().frameStats) {
        g_lastPresent = now;
        return false;
    }
    if (g_sampleCount < kMaxSamples) {
        g_samples[g_sampleCount++] = static_cast<float>(ToMs(now - g_lastPresent));
    }
    g_lastPresent = now;

    if (ToMs(now - g_windowStart) < GetConfig().reportIntervalSeconds * 1000.0) {
        return false;
    }
    try {
        Report(now);
    } catch (...) {
    }
    g_sampleCount = 0;
    g_windowStart = now;
    return true;
}

void OnDeviceReset() noexcept {
    g_lastPresent = 0;
    g_sampleCount = 0;
    g_nextDeadline = 0;
}

void LogFinalReport() noexcept {
    log::Info("Session : {} images présentées, {} saccades (>{:.0f} ms) mesurées", g_totalFrames, g_totalHitches,
              kHitchThresholdMs);
}

}  // namespace dpsf::frames
