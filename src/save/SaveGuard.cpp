#include "save/SaveGuard.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"

namespace dpsf::save {

namespace {

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CloseHandleFn = BOOL(WINAPI*)(HANDLE);
using WriteFileFn = BOOL(WINAPI*)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
using FlushFileBuffersFn = BOOL(WINAPI*)(HANDLE);
using DeleteFileAFn = BOOL(WINAPI*)(LPCSTR);

CreateFileAFn g_createFileA = nullptr;
CloseHandleFn g_closeHandle = nullptr;
WriteFileFn g_writeFile = nullptr;
FlushFileBuffersFn g_flushFileBuffers = nullptr;
DeleteFileAFn g_deleteFileA = nullptr;

// Une écriture de dp.sav en cours, du CreateFileA du jeu jusqu'à son CloseHandle.
struct TrackedWrite {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::wstring tempPath;  // vide si l'écriture va directement dans dp.sav (mode non atomique)
    std::uint64_t bytesWritten = 0;
    std::uint32_t writeCalls = 0;
    std::uint32_t checkpoints = 0;
    ULONGLONG openedAt = 0;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<TrackedWrite> g_tracked;
std::atomic<int> g_trackedCount{0};  // lecture sans verrou sur les chemins chauds (CloseHandle, WriteFile)

std::wstring g_savePath;
std::wstring g_tempPath;
std::wstring g_checkpointPath;

bool IsWriteAccess(DWORD access) noexcept {
    return (access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;
}

std::wstring FullPath(const std::wstring& path) {
    const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0) {
        return path;
    }
    std::wstring result(length, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), length, result.data(), nullptr);
    result.resize(written);
    return result;
}

bool FileExists(const std::wstring& path) noexcept {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::int64_t FileSize(const std::wstring& path) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return -1;
    }
    return (static_cast<std::int64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

const char* DispositionName(DWORD disposition) noexcept {
    switch (disposition) {
        case CREATE_NEW:
            return "CREATE_NEW";
        case CREATE_ALWAYS:
            return "CREATE_ALWAYS";
        case OPEN_EXISTING:
            return "OPEN_EXISTING";
        case OPEN_ALWAYS:
            return "OPEN_ALWAYS";
        case TRUNCATE_EXISTING:
            return "TRUNCATE_EXISTING";
        default:
            return "?";
    }
}

// Copie un fichier même s'il est encore ouvert en écriture par le jeu (CopyFileW refuserait :
// il n'ouvre pas la source avec FILE_SHARE_WRITE). Le contenu est forcé sur disque avant fermeture.
bool CopyFileShared(const std::wstring& source, const std::wstring& destination) noexcept {
    const HANDLE in = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in == INVALID_HANDLE_VALUE) {
        return false;
    }
    const HANDLE out =
        CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        CloseHandle(in);
        return false;
    }
    bool ok = true;
    char buffer[64 * 1024];
    while (true) {
        DWORD read = 0;
        if (!ReadFile(in, buffer, sizeof(buffer), &read, nullptr)) {
            ok = false;
            break;
        }
        if (read == 0) {
            break;
        }
        DWORD written = 0;
        if (!WriteFile(out, buffer, read, &written, nullptr) || written != read) {
            ok = false;
            break;
        }
    }
    ok = ok && FlushFileBuffers(out);
    CloseHandle(out);
    CloseHandle(in);
    if (!ok) {
        DeleteFileW(destination.c_str());
    }
    return ok;
}

// Remplace dp.sav par `source` en une seule opération (renommage NTFS sur le même volume).
bool ReplaceSaveWith(const std::wstring& source) noexcept {
    return MoveFileExW(source.c_str(), g_savePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void PruneBackups() {
    const Paths& paths = GetPaths();
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW data{};
    const HANDLE find = FindFirstFileW((paths.backupsDir + L"\\dp_*.sav").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        names.emplace_back(data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);

    // Noms préfixés par un horodatage triable : les plus anciens en tête.
    std::sort(names.begin(), names.end());
    const size_t keep = static_cast<size_t>(GetConfig().saveBackupCount);
    for (size_t i = 0; names.size() > keep && i < names.size() - keep; ++i) {
        DeleteFileW((paths.backupsDir + L"\\" + names[i]).c_str());
    }
}

void BackupSave(const wchar_t* reason) {
    if (!FileExists(g_savePath)) {
        return;
    }
    const std::wstring destination = GetPaths().backupsDir + L"\\dp_" + FileTimestamp() + L"_" + reason + L".sav";
    if (CopyFileShared(g_savePath, destination)) {
        log::Info("Copie de secours de dp.sav ({} octets) : {}", FileSize(destination), WideToUtf8(destination));
        PruneBackups();
    } else {
        log::Error("Échec de la copie de secours de dp.sav vers {} (erreur {})", WideToUtf8(destination),
                   GetLastError());
    }
}

// Un fichier temporaire restant signifie que la session précédente s'est arrêtée pendant une écriture
// de sauvegarde. dp.sav n'a alors pas été remplacé : on garde le fichier incomplet pour analyse.
void RecoverLeftoverTemp() {
    for (const std::wstring* leftover : {&g_tempPath, &g_checkpointPath}) {
        if (!FileExists(*leftover)) {
            continue;
        }
        const std::wstring destination =
            GetPaths().backupsDir + L"\\recovered_" + FileTimestamp() + L"_" +
            (leftover == &g_tempPath ? L"ecriture-interrompue" : L"checkpoint-interrompu") + L".sav";
        if (MoveFileExW(leftover->c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            log::Warn("Écriture de sauvegarde interrompue lors de la session précédente : dp.sav n'a pas été remplacé. "
                      "Fichier incomplet conservé : {}",
                      WideToUtf8(destination));
        } else {
            log::Error("Impossible de déplacer le fichier temporaire restant {} (erreur {})", WideToUtf8(*leftover),
                       GetLastError());
        }
    }
}

void Track(TrackedWrite write) {
    AcquireSRWLockExclusive(&g_lock);
    g_tracked.push_back(std::move(write));
    g_trackedCount.store(static_cast<int>(g_tracked.size()), std::memory_order_release);
    ReleaseSRWLockExclusive(&g_lock);
}

bool HasAtomicWriteInProgress() {
    AcquireSRWLockShared(&g_lock);
    const bool found = std::any_of(g_tracked.begin(), g_tracked.end(),
                                   [](const TrackedWrite& write) { return !write.tempPath.empty(); });
    ReleaseSRWLockShared(&g_lock);
    return found;
}

bool Untrack(HANDLE handle, TrackedWrite& out) {
    AcquireSRWLockExclusive(&g_lock);
    const auto it = std::find_if(g_tracked.begin(), g_tracked.end(),
                                 [handle](const TrackedWrite& write) { return write.handle == handle; });
    const bool found = it != g_tracked.end();
    if (found) {
        out = std::move(*it);
        g_tracked.erase(it);
        g_trackedCount.store(static_cast<int>(g_tracked.size()), std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_lock);
    return found;
}

void Commit(const TrackedWrite& write) {
    const ULONGLONG elapsed = GetTickCount64() - write.openedAt;
    if (write.tempPath.empty()) {
        log::Info("Écriture directe de dp.sav terminée : {} octets en {} appels, {} ms (taille finale {} octets)",
                  write.bytesWritten, write.writeCalls, elapsed, FileSize(g_savePath));
        return;
    }
    if (ReplaceSaveWith(write.tempPath)) {
        log::Info("Sauvegarde validée : {} octets en {} appels, {} ms, {} point(s) de contrôle (taille finale {} octets)",
                  write.bytesWritten, write.writeCalls, elapsed, write.checkpoints, FileSize(g_savePath));
    } else {
        log::Error("Échec du remplacement de dp.sav par {} (erreur {}). dp.sav précédent conservé ; le fichier "
                   "temporaire sera récupéré au prochain lancement.",
                   WideToUtf8(write.tempPath), GetLastError());
    }
}

// Point de contrôle : le jeu a demandé que ses données soient sur disque, on publie l'état courant
// de la copie temporaire dans dp.sav sans attendre la fermeture.
void Checkpoint(TrackedWrite& write) {
    if (!CopyFileShared(write.tempPath, g_checkpointPath)) {
        log::Error("Point de contrôle impossible : copie de {} échouée (erreur {})", WideToUtf8(write.tempPath),
                   GetLastError());
        return;
    }
    if (!ReplaceSaveWith(g_checkpointPath)) {
        log::Error("Point de contrôle impossible : remplacement de dp.sav échoué (erreur {})", GetLastError());
        DeleteFileW(g_checkpointPath.c_str());
        return;
    }
    ++write.checkpoints;
    log::Info("Point de contrôle de la sauvegarde ({} octets)", FileSize(g_savePath));
}

HANDLE OpenSaveForWrite(LPCSTR fileName, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                        DWORD disposition, DWORD flags, HANDLE templateFile) {
    log::Info("Le jeu ouvre dp.sav en écriture (accès 0x{:08X}, partage 0x{:X}, {}, attributs 0x{:08X})", access,
              share, DispositionName(disposition), flags);
    BackupSave(L"avant-ecriture");

    const bool atomic = GetConfig().atomicSaveWrites;
    if (atomic && HasAtomicWriteInProgress()) {
        log::Warn("dp.sav déjà ouvert en écriture : cette seconde ouverture n'est pas redirigée");
    } else if (atomic) {
        // La copie temporaire part du contenu actuel : si le jeu ne réécrit qu'une partie du fichier,
        // le résultat est identique à une écriture directe.
        const bool prepared = FileExists(g_savePath) ? CopyFileShared(g_savePath, g_tempPath)
                                                     : (DeleteFileW(g_tempPath.c_str()) || !FileExists(g_tempPath));
        if (prepared) {
            // FILE_SHARE_READ ajouté pour pouvoir copier la temporaire lors des points de contrôle.
            const HANDLE handle = CreateFileW(g_tempPath.c_str(), access, share | FILE_SHARE_READ, security,
                                              disposition, flags, templateFile);
            const DWORD error = GetLastError();
            if (handle != INVALID_HANDLE_VALUE) {
                Track({handle, g_tempPath, 0, 0, 0, GetTickCount64()});
                log::Info("Écriture redirigée vers {}", WideToUtf8(g_tempPath));
                SetLastError(error);
                return handle;
            }
            log::Error("Ouverture de la copie temporaire impossible (erreur {}) : écriture directe dans dp.sav",
                       error);
        } else {
            log::Error("Préparation de la copie temporaire impossible (erreur {}) : écriture directe dans dp.sav",
                       GetLastError());
        }
    }

    const HANDLE handle = g_createFileA(fileName, access, share, security, disposition, flags, templateFile);
    const DWORD error = GetLastError();
    if (handle != INVALID_HANDLE_VALUE) {
        Track({handle, {}, 0, 0, 0, GetTickCount64()});
    } else {
        log::Error("Le jeu n'a pas pu ouvrir dp.sav (erreur {})", error);
    }
    SetLastError(error);
    return handle;
}

HANDLE WINAPI HookCreateFileA(LPCSTR fileName, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                              DWORD disposition, DWORD flags, HANDLE templateFile) {
    if (fileName == nullptr) {
        return g_createFileA(fileName, access, share, security, disposition, flags, templateFile);
    }
    try {
        const std::wstring fullPath = FullPath(AnsiToWide(fileName));
        if (EqualsIgnoreCase(fullPath, g_savePath)) {
            if (IsWriteAccess(access)) {
                return OpenSaveForWrite(fileName, access, share, security, disposition, flags, templateFile);
            }
            const HANDLE handle = g_createFileA(fileName, access, share, security, disposition, flags, templateFile);
            const DWORD error = GetLastError();
            log::Info("Le jeu ouvre dp.sav en lecture ({} octets) : {}", FileSize(g_savePath),
                      handle != INVALID_HANDLE_VALUE ? "ok" : std::format("erreur {}", error));
            SetLastError(error);
            return handle;
        }
        if (GetConfig().logAllFileOpens) {
            const HANDLE handle = g_createFileA(fileName, access, share, security, disposition, flags, templateFile);
            const DWORD error = GetLastError();
            log::Info("CreateFileA {} (accès 0x{:08X}, {}) -> {}", WideToUtf8(fullPath), access,
                      DispositionName(disposition), handle != INVALID_HANDLE_VALUE ? "ok" : std::format("erreur {}", error));
            SetLastError(error);
            return handle;
        }
    } catch (...) {
        // Jamais d'exception vers le jeu : on retombe sur l'appel d'origine.
    }
    return g_createFileA(fileName, access, share, security, disposition, flags, templateFile);
}

BOOL WINAPI HookWriteFile(HANDLE file, LPCVOID buffer, DWORD bytesToWrite, LPDWORD bytesWritten,
                          LPOVERLAPPED overlapped) {
    const BOOL result = g_writeFile(file, buffer, bytesToWrite, bytesWritten, overlapped);
    if (g_trackedCount.load(std::memory_order_acquire) == 0) {
        return result;
    }
    const DWORD error = GetLastError();
    AcquireSRWLockExclusive(&g_lock);
    for (TrackedWrite& write : g_tracked) {
        if (write.handle == file) {
            ++write.writeCalls;
            if (result) {
                write.bytesWritten += bytesWritten != nullptr ? *bytesWritten : bytesToWrite;
            }
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    SetLastError(error);
    return result;
}

BOOL WINAPI HookFlushFileBuffers(HANDLE file) {
    const BOOL result = g_flushFileBuffers(file);
    if (g_trackedCount.load(std::memory_order_acquire) == 0) {
        return result;
    }
    const DWORD error = GetLastError();
    try {
        AcquireSRWLockExclusive(&g_lock);
        for (TrackedWrite& write : g_tracked) {
            if (write.handle == file && !write.tempPath.empty() && result) {
                Checkpoint(write);
                break;
            }
        }
        ReleaseSRWLockExclusive(&g_lock);
    } catch (...) {
        ReleaseSRWLockExclusive(&g_lock);
    }
    SetLastError(error);
    return result;
}

BOOL WINAPI HookCloseHandle(HANDLE handle) {
    if (g_trackedCount.load(std::memory_order_acquire) == 0) {
        return g_closeHandle(handle);
    }
    TrackedWrite write;
    const bool tracked = Untrack(handle, write);
    const BOOL result = g_closeHandle(handle);
    if (tracked) {
        const DWORD error = GetLastError();
        try {
            Commit(write);
        } catch (...) {
        }
        SetLastError(error);
    }
    return result;
}

BOOL WINAPI HookDeleteFileA(LPCSTR fileName) {
    try {
        if (fileName != nullptr && EqualsIgnoreCase(FullPath(AnsiToWide(fileName)), g_savePath)) {
            log::Warn("Le jeu supprime dp.sav : copie de secours préalable");
            BackupSave(L"avant-suppression");
        }
    } catch (...) {
    }
    return g_deleteFileA(fileName);
}

template <typename Fn>
void PatchKernel32(HMODULE gameModule, const char* name, Fn hook, Fn* original) {
    if (!hooking::PatchImport(gameModule, "kernel32.dll", name, hook, original)) {
        log::Error("Interception de {} impossible : protection des sauvegardes incomplète", name);
    }
}

}  // namespace

void Install(HMODULE gameModule) {
    const Config& config = GetConfig();
    if (!config.saveGuard) {
        log::Info("Protection des sauvegardes désactivée (SaveGuard=0)");
        return;
    }
    g_savePath = FullPath(GetPaths().gameDir + L"\\" + config.saveFile);
    g_tempPath = g_savePath + L".dpsf.tmp";
    g_checkpointPath = g_savePath + L".dpsf.ckpt";
    const std::int64_t size = FileSize(g_savePath);
    log::Info("Sauvegarde surveillée : {} ({}) | écriture atomique : {} | copies conservées : {}",
              WideToUtf8(g_savePath), size >= 0 ? std::format("{} octets", size) : std::string("absente"),
              config.atomicSaveWrites ? "oui" : "non", config.saveBackupCount);

    RecoverLeftoverTemp();
    BackupSave(L"demarrage");

    PatchKernel32(gameModule, "CreateFileA", &HookCreateFileA, &g_createFileA);
    PatchKernel32(gameModule, "WriteFile", &HookWriteFile, &g_writeFile);
    PatchKernel32(gameModule, "FlushFileBuffers", &HookFlushFileBuffers, &g_flushFileBuffers);
    PatchKernel32(gameModule, "CloseHandle", &HookCloseHandle, &g_closeHandle);
    PatchKernel32(gameModule, "DeleteFileA", &HookDeleteFileA, &g_deleteFileA);
}

void OnProcessExit() noexcept {
    if (g_trackedCount.load(std::memory_order_acquire) == 0) {
        return;
    }
    // Le jeu se ferme sans avoir fermé dp.sav : on publie ce qui a été écrit, comme l'aurait fait
    // l'écriture directe d'origine.
    try {
        AcquireSRWLockExclusive(&g_lock);
        for (TrackedWrite& write : g_tracked) {
            log::Warn("Fermeture du jeu avec dp.sav encore ouvert en écriture ({} octets écrits)", write.bytesWritten);
            if (!write.tempPath.empty()) {
                g_flushFileBuffers(write.handle);
                Checkpoint(write);
            }
        }
        ReleaseSRWLockExclusive(&g_lock);
    } catch (...) {
        ReleaseSRWLockExclusive(&g_lock);
    }
}

void LogCrashState() noexcept {
    const int count = g_trackedCount.load(std::memory_order_acquire);
    if (count == 0) {
        log::Crash("Aucune écriture de sauvegarde en cours au moment du plantage");
        return;
    }
    if (!TryAcquireSRWLockShared(&g_lock)) {
        log::Crash("{} écriture(s) de sauvegarde en cours au moment du plantage (détails indisponibles)", count);
        return;
    }
    for (const TrackedWrite& write : g_tracked) {
        if (write.tempPath.empty()) {
            log::Crash("Plantage PENDANT une écriture directe de dp.sav ({} octets écrits) : le fichier est "
                       "probablement corrompu, voir les copies de secours",
                       write.bytesWritten);
        } else {
            log::Crash("Plantage pendant une écriture de sauvegarde ({} octets écrits, {} point(s) de contrôle) : "
                       "dp.sav n'a pas été touché par cette écriture",
                       write.bytesWritten, write.checkpoints);
        }
    }
    ReleaseSRWLockShared(&g_lock);
}

}  // namespace dpsf::save
