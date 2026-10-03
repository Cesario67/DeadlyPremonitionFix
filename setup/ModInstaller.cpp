#include "ModInstaller.h"

#include <windows.h>

#include <vector>

namespace setup {

namespace {

constexpr const wchar_t* kDllName = L"X3DAudio1_7.dll";
constexpr const wchar_t* kIniName = L"DPStabilityFix.ini";

bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Description de la ressource de version (« DPStabilityFix ... » pour notre DLL).
std::wstring FileDescription(const std::wstring& path) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return {};
    }
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
        return {};
    }
    struct Translation {
        WORD language;
        WORD codePage;
    };
    Translation* translations = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) ||
        length < sizeof(Translation)) {
        return {};
    }
    wchar_t query[64]{};
    swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\FileDescription", translations[0].language,
               translations[0].codePage);
    wchar_t* description = nullptr;
    if (!VerQueryValueW(data.data(), query, reinterpret_cast<void**>(&description), &length) || length == 0) {
        return {};
    }
    return description;
}

bool IsOurDll(const std::wstring& path) {
    return FileDescription(path).find(L"DPStabilityFix") != std::wstring::npos;
}

std::wstring ErrorText(DWORD error) {
    if (error == ERROR_ACCESS_DENIED) {
        return L"accès refusé (relancer l'installeur en administrateur si le jeu est dans Program Files)";
    }
    if (error == ERROR_SHARING_VIOLATION) {
        return L"fichier utilisé (fermer le jeu)";
    }
    return L"erreur Windows " + std::to_wstring(error);
}

}  // namespace

Result InstallMod(const std::wstring& gameDir, const std::wstring& sourceDir) {
    const std::wstring sourceDll = sourceDir + L"\\" + kDllName;
    const std::wstring sourceIni = sourceDir + L"\\" + kIniName;
    if (!FileExists(sourceDll) || !IsOurDll(sourceDll)) {
        return Failure(L"X3DAudio1_7.dll du mod introuvable à côté de l'installeur.");
    }

    std::wstring report;
    const std::wstring targetDll = gameDir + L"\\" + kDllName;
    if (FileExists(targetDll) && !IsOurDll(targetDll)) {
        if (!CopyFileW(targetDll.c_str(), (targetDll + L".avant-dpsf").c_str(), FALSE)) {
            return Failure(L"Mise de côté de la X3DAudio1_7.dll existante impossible : " + ErrorText(GetLastError()));
        }
        report += L"X3DAudio1_7.dll existante conservée sous X3DAudio1_7.dll.avant-dpsf.\n";
    }
    if (!CopyFileW(sourceDll.c_str(), targetDll.c_str(), FALSE)) {
        return Failure(L"Copie de X3DAudio1_7.dll impossible : " + ErrorText(GetLastError()));
    }
    report += L"Mod installé (X3DAudio1_7.dll).";

    const std::wstring targetIni = gameDir + L"\\" + kIniName;
    if (FileExists(targetIni)) {
        report += L"\nDPStabilityFix.ini existant conservé.";
    } else if (FileExists(sourceIni) && CopyFileW(sourceIni.c_str(), targetIni.c_str(), TRUE)) {
        report += L"\nDPStabilityFix.ini installé.";
    } else {
        report += L"\nDPStabilityFix.ini absent : le mod utilisera ses réglages par défaut.";
    }
    return Success(report);
}

}  // namespace setup
