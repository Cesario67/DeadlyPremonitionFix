#include "core/Config.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>

namespace dpsf {

namespace {

Config g_config;

int ReadInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int fallback, int minValue,
            int maxValue) {
    const int value = static_cast<int>(GetPrivateProfileIntW(section, key, fallback, ini.c_str()));
    return std::clamp(value, minValue, maxValue);
}

bool ReadBool(const std::wstring& ini, const wchar_t* section, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, ini.c_str()) != 0;
}

std::wstring ReadString(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
                        const std::wstring& fallback) {
    wchar_t buffer[MAX_PATH]{};
    GetPrivateProfileStringW(section, key, fallback.c_str(), buffer, MAX_PATH, ini.c_str());
    return buffer[0] != L'\0' ? std::wstring(buffer) : fallback;
}

// Nombre décimal (point ou virgule), borné. Valeur absente ou illisible : `fallback`.
float ReadFloat(const std::wstring& ini, const wchar_t* section, const wchar_t* key, float fallback, float minValue,
                float maxValue) {
    std::wstring text = ReadString(ini, section, key, L"");
    if (text.empty()) {
        return fallback;
    }
    std::replace(text.begin(), text.end(), L',', L'.');
    wchar_t* end = nullptr;
    const float value = std::wcstof(text.c_str(), &end);
    if (end == text.c_str()) {
        return fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

}  // namespace

bool LoadConfig(const std::wstring& iniPath) {
    const Config defaults;
    Config config;

    config.saveGuard = ReadBool(iniPath, L"Save", L"SaveGuard", defaults.saveGuard);
    config.atomicSaveWrites = ReadBool(iniPath, L"Save", L"AtomicWrites", defaults.atomicSaveWrites);
    config.saveBackupCount = ReadInt(iniPath, L"Save", L"BackupCount", defaults.saveBackupCount, 1, 500);
    config.saveFile = ReadString(iniPath, L"Save", L"SaveFile", defaults.saveFile);

    config.crashDumps = ReadBool(iniPath, L"Crash", L"CrashDumps", defaults.crashDumps);
    config.fullMemoryDumps = ReadBool(iniPath, L"Crash", L"FullMemoryDumps", defaults.fullMemoryDumps);
    config.dumpKeepCount = ReadInt(iniPath, L"Crash", L"DumpKeepCount", defaults.dumpKeepCount, 1, 100);

    config.frameStats = ReadBool(iniPath, L"Frames", L"FrameStats", defaults.frameStats);
    config.reportIntervalSeconds =
        ReadInt(iniPath, L"Frames", L"ReportIntervalSeconds", defaults.reportIntervalSeconds, 1, 3600);
    config.timerResolutionMs = ReadInt(iniPath, L"Frames", L"TimerResolutionMs", defaults.timerResolutionMs, 0, 15);
    config.frameLimitFps = ReadInt(iniPath, L"Frames", L"FrameLimitFps", defaults.frameLimitFps, 0, 1000);
    config.preciseGameTime = ReadBool(iniPath, L"Frames", L"PreciseGameTime", defaults.preciseGameTime);
    config.forceFpuPreserve = ReadBool(iniPath, L"Frames", L"ForceFpuPreserve", defaults.forceFpuPreserve);

    config.aimPrecisionGuard = ReadBool(iniPath, L"Gameplay", L"AimPrecisionGuard", defaults.aimPrecisionGuard);
    config.skipIntro = ReadBool(iniPath, L"Gameplay", L"SkipIntro", defaults.skipIntro);
    config.zeroDeltaGuard = ReadBool(iniPath, L"Gameplay", L"ZeroDeltaGuard", defaults.zeroDeltaGuard);

    config.integratedDpfix = ReadBool(iniPath, L"Graphics", L"IntegratedDPfix", defaults.integratedDpfix);
    config.drawDistanceScale = ReadFloat(iniPath, L"Graphics", L"DrawDistanceScale", defaults.drawDistanceScale, 1.0f, 8.0f);

    config.sonyControllerLayout = ReadBool(iniPath, L"Controller", L"SonyLayout", defaults.sonyControllerLayout);
    config.controllerDiagnostics = ReadBool(iniPath, L"Controller", L"Diagnostics", defaults.controllerDiagnostics);
    config.swapControllerTriggers = ReadBool(iniPath, L"Controller", L"SwapTriggers", defaults.swapControllerTriggers);
    config.backgroundControllerPolling =
        ReadBool(iniPath, L"Controller", L"BackgroundPolling", defaults.backgroundControllerPolling);

    config.dpfixResetWorkaround =
        ReadBool(iniPath, L"Compat", L"DPfixResetWorkaround", defaults.dpfixResetWorkaround);

    config.logAllFileOpens =ReadBool(iniPath, L"Debug", L"LogAllFileOpens", defaults.logAllFileOpens);
    config.renderDiagnostics = ReadBool(iniPath, L"Debug", L"RenderDiagnostics", defaults.renderDiagnostics);
    config.logKeepCount = ReadInt(iniPath, L"Debug", L"LogKeepCount", defaults.logKeepCount, 1, 500);

    g_config = config;
    return GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Config& GetConfig() noexcept {
    return g_config;
}

}  // namespace dpsf
