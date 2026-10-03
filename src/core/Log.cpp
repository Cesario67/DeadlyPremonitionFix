#include "core/Log.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "core/Strings.h"

namespace dpsf::log {

namespace {

HANDLE g_file = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_lock;
bool g_lockInitialized = false;

const char* LevelName(Level level) noexcept {
    switch (level) {
        case Level::Info:
            return "INFO ";
        case Level::Warn:
            return "WARN ";
        case Level::Error:
            return "ERROR";
    }
    return "?    ";
}

void PruneOldLogs(const std::wstring& logsDir, int keepCount) {
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW data{};
    const HANDLE find = FindFirstFileW((logsDir + L"\\DPStabilityFix_*.log").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        names.emplace_back(data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);

    // Les noms commencent par un horodatage triable : les plus anciens sont en tête.
    std::sort(names.begin(), names.end());
    const size_t keep = static_cast<size_t>(keepCount);
    for (size_t i = 0; names.size() > keep && i < names.size() - keep; ++i) {
        DeleteFileW((logsDir + L"\\" + names[i]).c_str());
    }
}

// Écrit la ligne sans prendre le verrou : l'appelant s'en charge (ou y renonce en cas de plantage).
void WriteUnlocked(Level level, std::string_view message) noexcept {
    if (g_file == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char prefix[96]{};
    const int prefixLength =
        sprintf_s(prefix, "%04u-%02u-%02u %02u:%02u:%02u.%03u [tid %5lu] %s ", now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, GetCurrentThreadId(), LevelName(level));
    DWORD written = 0;
    if (prefixLength > 0) {
        WriteFile(g_file, prefix, static_cast<DWORD>(prefixLength), &written, nullptr);
    }
    WriteFile(g_file, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
    WriteFile(g_file, "\r\n", 2, &written, nullptr);
}

}  // namespace

bool Open(const std::wstring& logsDir, int keepCount) {
    if (!g_lockInitialized) {
        InitializeCriticalSection(&g_lock);
        g_lockInitialized = true;
    }
    PruneOldLogs(logsDir, std::max(keepCount - 1, 0));

    const std::wstring path = logsDir + L"\\DPStabilityFix_" + FileTimestamp() + L".log";
    g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file == INVALID_HANDLE_VALUE) {
        return false;
    }
    // BOM UTF-8 : le Bloc-notes affiche correctement les accents.
    DWORD written = 0;
    WriteFile(g_file, "\xEF\xBB\xBF", 3, &written, nullptr);
    return true;
}

void Close() noexcept {
    if (g_file == INVALID_HANDLE_VALUE) {
        return;
    }
    EnterCriticalSection(&g_lock);
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&g_lock);
}

void Write(Level level, std::string_view message) noexcept {
    if (!g_lockInitialized) {
        return;
    }
    EnterCriticalSection(&g_lock);
    WriteUnlocked(level, message);
    LeaveCriticalSection(&g_lock);
}

void WriteEmergency(Level level, std::string_view message) noexcept {
    if (!g_lockInitialized) {
        return;
    }
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (TryEnterCriticalSection(&g_lock)) {
            WriteUnlocked(level, message);
            LeaveCriticalSection(&g_lock);
            return;
        }
        ::Sleep(2);
    }
    // Le verrou est probablement détenu par le thread qui a planté : on écrit quand même.
    WriteUnlocked(level, message);
}

}  // namespace dpsf::log
