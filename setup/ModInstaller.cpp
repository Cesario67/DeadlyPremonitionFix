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

// Copie un fichier de réglages s'il n'existe pas encore dans le jeu (les réglages de l'utilisateur
// sont conservés).
std::wstring InstallSettingsFile(const std::wstring& gameDir, const std::wstring& sourceDir, const wchar_t* name) {
    const std::wstring source = sourceDir + L"\\" + name;
    const std::wstring target = gameDir + L"\\" + name;
    if (FileExists(target)) {
        return std::wstring(name) + L" existant conservé.";
    }
    if (FileExists(source) && CopyFileW(source.c_str(), target.c_str(), TRUE)) {
        return std::wstring(name) + L" installé.";
    }
    return std::wstring(name) + L" absent : réglages par défaut.";
}

// Copie les fichiers (sans sous-dossiers) de `source` vers `target`. Renvoie le nombre de fichiers
// copiés, ou -1 en cas d'erreur.
int CopyFlatDirectory(const std::wstring& source, const std::wstring& target) {
    if (!CreateDirectoryW(target.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return -1;
    }
    WIN32_FIND_DATAW data{};
    const HANDLE find = FindFirstFileW((source + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return -1;
    }
    int copied = 0;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        const std::wstring name = data.cFileName;
        if (!CopyFileW((source + L"\\" + name).c_str(), (target + L"\\" + name).c_str(), FALSE)) {
            FindClose(find);
            return -1;
        }
        ++copied;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return copied;
}

}  // namespace

Result InstallMod(const std::wstring& gameDir, const std::wstring& sourceDir) {
    const std::wstring sourceDll = sourceDir + L"\\" + kDllName;
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

    report += L"\n" + InstallSettingsFile(gameDir, sourceDir, kIniName);

    // DPfix intégré : ses réglages (conservés s'ils existent, y compris ceux d'un DPfix d'origine) et
    // ses shaders (toujours mis à jour).
    report += L"\n" + InstallSettingsFile(gameDir, sourceDir, L"DPfix.ini");
    report += L"\n" + InstallSettingsFile(gameDir, sourceDir, L"DPfixKeys.ini");
    const int shaders = CopyFlatDirectory(sourceDir + L"\\dpfix", gameDir + L"\\dpfix");
    if (shaders < 0) {
        return Failure(L"Copie des shaders de DPfix (dossier dpfix) impossible : " + ErrorText(GetLastError()));
    }
    report += L"\nShaders de DPfix installés (" + std::to_wstring(shaders) + L" fichiers).";
    return Success(report);
}

bool HasExternalDpfix(const std::wstring& gameDir) {
    return FileExists(gameDir + L"\\d3d9.dll");
}

Result DisableExternalDpfix(const std::wstring& gameDir) {
    const std::wstring dll = gameDir + L"\\d3d9.dll";
    const std::wstring renamed = dll + L".dpfix-desactive";
    if (!MoveFileExW(dll.c_str(), renamed.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        return Failure(L"Impossible de renommer d3d9.dll : " + ErrorText(GetLastError()));
    }
    return Success(L"DPfix d'origine désactivé (d3d9.dll renommé en d3d9.dll.dpfix-desactive).");
}

}  // namespace setup
