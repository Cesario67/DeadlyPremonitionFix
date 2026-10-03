// DPStabilityFixSetup.exe : installe DPStabilityFix et applique le patch 4 Go.
//
// Usage :
// - double-clic : une fenêtre demande de sélectionner DP.exe ;
// - glisser DP.exe (ou le dossier du jeu) sur l'installeur ;
// - ligne de commande : DPStabilityFixSetup.exe "<chemin de DP.exe ou du dossier>" [--quiet]
//   (--quiet : aucune fenêtre, résultat dans le code de sortie, 0 = succès).
// La DLL et le .ini du mod doivent se trouver à côté de l'installeur.

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <string>

#include "ExePatcher.h"
#include "ModInstaller.h"

namespace {

constexpr const wchar_t* kTitle = L"DPStabilityFix : installation";

struct Options {
    std::wstring target;  // DP.exe ou dossier du jeu, vide = demander
    bool quiet = false;
};

Options ParseArguments() {
    Options options;
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 1; arguments != nullptr && i < count; ++i) {
        const std::wstring argument = arguments[i];
        if (argument == L"--quiet") {
            options.quiet = true;
        } else {
            options.target = argument;
        }
    }
    LocalFree(arguments);
    return options;
}

std::wstring AskForGameExe() {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = L"Deadly Premonition (DP.exe)\0DP.exe\0Exécutables (*.exe)\0*.exe\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = L"Sélectionner DP.exe (dossier d'installation de Deadly Premonition)";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) ? std::wstring(path) : std::wstring();
}

bool IsDirectory(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring FileName(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

std::wstring ParentDirectory(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring(L".") : path.substr(0, separator);
}

std::wstring OwnDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return ParentDirectory(path);
}

class Installer {
public:
    explicit Installer(bool quiet) : quiet_(quiet) {}

    int Fail(const std::wstring& message) const {
        if (!quiet_) {
            MessageBoxW(nullptr, message.c_str(), kTitle, MB_ICONERROR | MB_OK);
        }
        return 1;
    }

    bool Confirm(const std::wstring& message) const {
        return quiet_ || MessageBoxW(nullptr, message.c_str(), kTitle, MB_ICONWARNING | MB_YESNO) == IDYES;
    }

    int Run(std::wstring target) const {
        if (target.empty()) {
            if (quiet_) {
                return Fail(L"Aucun chemin indiqué.");
            }
            target = AskForGameExe();
            if (target.empty()) {
                return 1;  // annulé par l'utilisateur
            }
        }
        while (!target.empty() && (target.back() == L'\\' || target.back() == L'/')) {
            target.pop_back();
        }
        const std::wstring exePath = IsDirectory(target) ? target + L"\\DP.exe" : target;
        if (CompareStringOrdinal(FileName(exePath).c_str(), -1, L"DP.exe", -1, TRUE) != CSTR_EQUAL) {
            return Fail(L"Le fichier sélectionné n'est pas DP.exe :\n" + exePath);
        }

        const setup::ExeInfo info = setup::ReadExeInfo(exePath);
        if (!info.valid) {
            return Fail(L"DP.exe introuvable ou illisible :\n" + exePath);
        }
        if (!info.x86) {
            return Fail(L"Ce DP.exe n'est pas l'exécutable 32 bits attendu de Deadly Premonition.");
        }
        if (info.timestamp != setup::kKnownGameTimestamp &&
            !Confirm(L"Ce DP.exe n'est pas la version Steam 1.01b pour laquelle le mod a été conçu.\n"
                     L"Installer quand même ?")) {
            return 1;
        }
        if (setup::IsFileInUse(exePath)) {
            return Fail(L"Le jeu semble lancé : fermez-le puis relancez l'installation.");
        }

        const setup::Result patch = setup::EnableLargeAddressAware(exePath);
        if (!patch.ok) {
            return Fail(patch.message);
        }
        const setup::Result install = setup::InstallMod(ParentDirectory(exePath), OwnDirectory());
        if (!install.ok) {
            return Fail(patch.message + L"\n\nMais : " + install.message);
        }

        if (!quiet_) {
            const std::wstring summary =
                patch.message + L"\n" + install.message +
                L"\n\nLancez le jeu normalement depuis Steam. Journaux, diagnostics et copies de secours "
                L"des sauvegardes : dossier DPStabilityFix à côté de DP.exe."
                L"\n\nPour tout annuler : supprimer X3DAudio1_7.dll, puis remplacer DP.exe par "
                L"DP.exe.dpsf-original (renommé en DP.exe).";
            MessageBoxW(nullptr, summary.c_str(), kTitle, MB_ICONINFORMATION | MB_OK);
        }
        return 0;
    }

private:
    bool quiet_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const Options options = ParseArguments();
    return Installer(options.quiet).Run(options.target);
}
