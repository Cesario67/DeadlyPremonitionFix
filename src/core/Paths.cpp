#include "core/Paths.h"

#include <windows.h>

namespace dpsf {

namespace {

Paths g_paths;

std::wstring ModulePath(HMODULE module) {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        if (buffer.size() >= 32768) {
            return {};
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

}  // namespace

bool InitPaths() {
    g_paths.gameExe = ModulePath(nullptr);
    const size_t separator = g_paths.gameExe.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return false;
    }
    g_paths.gameDir = g_paths.gameExe.substr(0, separator);
    g_paths.iniFile = g_paths.gameDir + L"\\DPStabilityFix.ini";
    g_paths.modDir = g_paths.gameDir + L"\\DPStabilityFix";
    g_paths.logsDir = g_paths.modDir + L"\\logs";
    g_paths.dumpsDir = g_paths.modDir + L"\\crashdumps";
    g_paths.backupsDir = g_paths.modDir + L"\\savebackups";

    return EnsureDirectory(g_paths.modDir) && EnsureDirectory(g_paths.logsDir) &&
           EnsureDirectory(g_paths.dumpsDir) && EnsureDirectory(g_paths.backupsDir);
}

const Paths& GetPaths() noexcept {
    return g_paths;
}

}  // namespace dpsf
