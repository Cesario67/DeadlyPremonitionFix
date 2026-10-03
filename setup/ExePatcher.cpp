#include "ExePatcher.h"

#include <vector>

namespace setup {

namespace {

// Offset de IMAGE_FILE_HEADER.Characteristics dans le fichier, ou 0 si l'en-tête est invalide.
size_t CharacteristicsOffset(const std::vector<BYTE>& bytes) {
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        return 0;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return 0;
    }
    const size_t peOffset = static_cast<size_t>(dos->e_lfanew);
    if (peOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) > bytes.size()) {
        return 0;
    }
    if (*reinterpret_cast<const DWORD*>(bytes.data() + peOffset) != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    return peOffset + sizeof(DWORD) + offsetof(IMAGE_FILE_HEADER, Characteristics);
}

bool ReadWholeFile(const std::wstring& path, std::vector<BYTE>& bytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 256ll * 1024 * 1024;
    if (ok) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
    }
    CloseHandle(file);
    return ok;
}

bool WriteWholeFile(const std::wstring& path, const std::vector<BYTE>& bytes) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                    written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

std::wstring ErrorText(DWORD error) {
    if (error == ERROR_ACCESS_DENIED) {
        return L"accès refusé (relancer l'installeur en administrateur si le jeu est dans Program Files)";
    }
    return L"erreur Windows " + std::to_wstring(error);
}

}  // namespace

ExeInfo ReadExeInfo(const std::wstring& exePath) {
    ExeInfo info;
    std::vector<BYTE> bytes;
    if (!ReadWholeFile(exePath, bytes)) {
        return info;
    }
    const size_t offset = CharacteristicsOffset(bytes);
    if (offset == 0) {
        return info;
    }
    const auto* header = reinterpret_cast<const IMAGE_FILE_HEADER*>(bytes.data() + offset -
                                                                     offsetof(IMAGE_FILE_HEADER, Characteristics));
    info.valid = true;
    info.x86 = header->Machine == IMAGE_FILE_MACHINE_I386;
    info.timestamp = header->TimeDateStamp;
    info.largeAddressAware = (header->Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
    return info;
}

bool IsFileInUse(const std::wstring& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_SHARING_VIOLATION;
    }
    CloseHandle(file);
    return false;
}

Result EnableLargeAddressAware(const std::wstring& exePath) {
    std::vector<BYTE> bytes;
    if (!ReadWholeFile(exePath, bytes)) {
        return Failure(L"Lecture de DP.exe impossible : " + ErrorText(GetLastError()));
    }
    const size_t offset = CharacteristicsOffset(bytes);
    if (offset == 0) {
        return Failure(L"DP.exe n'a pas un en-tête d'exécutable valide.");
    }
    auto& characteristics = *reinterpret_cast<WORD*>(bytes.data() + offset);
    if ((characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0) {
        return Success(L"Patch 4 Go : déjà appliqué.");
    }

    // Copie de l'original, une seule fois : une réinstallation ne doit pas écraser la vraie copie d'origine.
    const std::wstring backup = exePath + L".dpsf-original";
    if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES &&
        !CopyFileW(exePath.c_str(), backup.c_str(), TRUE)) {
        return Failure(L"Copie de sauvegarde de DP.exe impossible : " + ErrorText(GetLastError()));
    }

    characteristics = static_cast<WORD>(characteristics | IMAGE_FILE_LARGE_ADDRESS_AWARE);
    const std::wstring temp = exePath + L".dpsf-tmp";
    if (!WriteWholeFile(temp, bytes)) {
        const DWORD error = GetLastError();
        DeleteFileW(temp.c_str());
        return Failure(L"Écriture de DP.exe modifié impossible : " + ErrorText(error));
    }
    if (!MoveFileExW(temp.c_str(), exePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temp.c_str());
        return Failure(L"Remplacement de DP.exe impossible : " + ErrorText(error));
    }
    if (!ReadExeInfo(exePath).largeAddressAware) {
        return Failure(L"Patch 4 Go : vérification échouée après écriture.");
    }
    return Success(L"Patch 4 Go appliqué (original conservé : DP.exe.dpsf-original).");
}

}  // namespace setup
