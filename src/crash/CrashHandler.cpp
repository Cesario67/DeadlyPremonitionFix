#include "crash/CrashHandler.h"

// dbghelp.h a besoin des définitions de windows.h avant lui.
#include <dbghelp.h>

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
#include "core/SystemInfo.h"
#include "save/SaveGuard.h"

namespace dpsf::crash {

namespace {

using SetUnhandledExceptionFilterFn = LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI*)(LPTOP_LEVEL_EXCEPTION_FILTER);
using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

constexpr int kStackWordsScanned = 512;
constexpr int kMaxStackCandidates = 32;

SetUnhandledExceptionFilterFn g_gameSetFilter = nullptr;  // pointeur d'origine de l'IAT de DP.exe
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;  // filtre en place avant le nôtre
LPTOP_LEVEL_EXCEPTION_FILTER g_gameFilter = nullptr;      // filtre que le jeu a voulu installer
std::atomic<HMODULE> g_dbghelp{nullptr};
std::atomic<bool> g_handling{false};

LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* exception);

const char* ExceptionName(DWORD code) noexcept {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_PRIV_INSTRUCTION:
            return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INVALID_OPERATION:
            return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
        case 0xC0000374:
            return "STATUS_HEAP_CORRUPTION";
        case 0xC0000017:
            return "STATUS_NO_MEMORY";
        case 0xE06D7363:
            return "exception C++ (0xE06D7363)";
        default:
            return "exception inconnue";
    }
}

HMODULE LoadDbgHelp() noexcept {
    HMODULE module = g_dbghelp.load();
    if (module != nullptr) {
        return module;
    }
    // Toujours celle du système : un dbghelp.dll déposé dans le dossier du jeu serait prioritaire.
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length + 13 >= MAX_PATH) {
        return nullptr;
    }
    wcscat_s(path, L"\\dbghelp.dll");
    module = LoadLibraryW(path);
    g_dbghelp.store(module);
    return module;
}

void PruneOldDumps() {
    const Paths& paths = GetPaths();
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW data{};
    const HANDLE find = FindFirstFileW((paths.dumpsDir + L"\\DP_*.dmp").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        names.emplace_back(data.cFileName);
    } while (FindNextFileW(find, &data));
    FindClose(find);
    std::sort(names.begin(), names.end());
    const size_t keep = static_cast<size_t>(std::max(GetConfig().dumpKeepCount - 1, 0));
    for (size_t i = 0; names.size() > keep && i < names.size() - keep; ++i) {
        DeleteFileW((paths.dumpsDir + L"\\" + names[i]).c_str());
    }
}

struct DumpJob {
    EXCEPTION_POINTERS* exception = nullptr;
    DWORD threadId = 0;
    const wchar_t* path = nullptr;
    bool fullMemory = false;
    BOOL succeeded = FALSE;
    DWORD error = 0;
};

// Le dump est écrit depuis un autre thread : celui qui a planté peut avoir une pile épuisée
// (dépassement de pile) ou un état incohérent.
DWORD WINAPI DumpThread(LPVOID parameter) {
    auto* job = static_cast<DumpJob*>(parameter);
    const HMODULE dbghelp = LoadDbgHelp();
    const auto writeDump =
        dbghelp != nullptr ? reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump")) : nullptr;
    if (writeDump == nullptr) {
        job->error = ERROR_PROC_NOT_FOUND;
        return 0;
    }
    const HANDLE file =
        CreateFileW(job->path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        job->error = GetLastError();
        return 0;
    }
    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = job->threadId;
    info.ExceptionPointers = job->exception;
    info.ClientPointers = FALSE;
    // DataSegs inclut la section .data de DP.exe (~12 Mo) : l'état global du jeu au moment du plantage.
    const auto type = static_cast<MINIDUMP_TYPE>(
        job->fullMemory ? (MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo |
                           MiniDumpWithUnloadedModules)
                        : (MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithHandleData |
                           MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData));
    job->succeeded = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr);
    if (!job->succeeded) {
        job->error = GetLastError();
    }
    CloseHandle(file);
    return 0;
}

// Copie brute d'une partie de la pile, protégée par SEH : pas d'objet C++ dans cette fonction.
int CopyStackWords(const std::uintptr_t* stack, std::uintptr_t* out, int count) noexcept {
    int copied = 0;
    __try {
        for (; copied < count; ++copied) {
            out[copied] = stack[copied];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return copied;
}

bool IsExecutableImageAddress(std::uintptr_t value) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(value), &info, sizeof(info)) == 0) {
        return false;
    }
    constexpr DWORD kExecutable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return info.State == MEM_COMMIT && info.Type == MEM_IMAGE && (info.Protect & kExecutable) != 0;
}

// Liste les valeurs de la pile qui pointent dans du code : candidates « adresses de retour », pour
// situer l'appelant sans symboles. Approximatif, mais suffisant pour orienter le désassemblage.
void LogStackScan(const CONTEXT& context) {
    std::uintptr_t words[kStackWordsScanned]{};
    const int copied = CopyStackWords(reinterpret_cast<const std::uintptr_t*>(context.Esp), words, kStackWordsScanned);
    int found = 0;
    for (int i = 0; i < copied && found < kMaxStackCandidates; ++i) {
        if (IsExecutableImageAddress(words[i])) {
            log::Crash("  pile [esp+0x{:03X}] {}", i * 4, sysinfo::FormatAddress(words[i]));
            ++found;
        }
    }
    if (found == 0) {
        log::Crash("  (aucune adresse de code trouvée dans les {} premiers mots de pile)", copied);
    }
}

void LogException(const EXCEPTION_POINTERS& exception) {
    const EXCEPTION_RECORD& record = *exception.ExceptionRecord;
    const CONTEXT& context = *exception.ContextRecord;
    log::Crash("=== PLANTAGE : {} (0x{:08X}) ===", ExceptionName(record.ExceptionCode), record.ExceptionCode);
    log::Crash("Adresse : {}", sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(record.ExceptionAddress)));
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
        const ULONG_PTR kind = record.ExceptionInformation[0];
        const char* operation = kind == 0 ? "lecture" : kind == 1 ? "écriture" : kind == 8 ? "exécution (DEP)" : "?";
        log::Crash("Accès invalide en {} à l'adresse 0x{:08X}", operation, record.ExceptionInformation[1]);
    }
    log::Crash("EAX={:08X} EBX={:08X} ECX={:08X} EDX={:08X} ESI={:08X} EDI={:08X}", context.Eax, context.Ebx,
               context.Ecx, context.Edx, context.Esi, context.Edi);
    log::Crash("EIP={:08X} ESP={:08X} EBP={:08X} EFLAGS={:08X}", context.Eip, context.Esp, context.Ebp,
               context.EFlags);
    LogStackScan(context);

    const sysinfo::MemorySnapshot memory = sysinfo::QueryMemory();
    log::Crash("Mémoire : espace d'adressage {}/{} Mo, privée {} Mo, RAM libre {} Mo", memory.addressSpaceUsedMb,
               memory.addressSpaceTotalMb, memory.privateMb, memory.physicalAvailableMb);
}

void WriteDump(EXCEPTION_POINTERS* exception) {
    const std::wstring path = GetPaths().dumpsDir + L"\\DP_" + FileTimestamp() + L".dmp";
    DumpJob job;
    job.exception = exception;
    job.threadId = GetCurrentThreadId();
    job.path = path.c_str();
    job.fullMemory = GetConfig().fullMemoryDumps;

    const HANDLE thread = CreateThread(nullptr, 0, &DumpThread, &job, 0, nullptr);
    if (thread == nullptr) {
        log::Crash("Impossible de créer le thread d'écriture du dump (erreur {})", GetLastError());
        return;
    }
    WaitForSingleObject(thread, 60000);
    CloseHandle(thread);
    if (job.succeeded) {
        log::Crash("Fichier de diagnostic écrit : {}", WideToUtf8(path));
    } else {
        log::Crash("Échec de l'écriture du fichier de diagnostic (erreur {})", job.error);
    }
}

LONG ChainToNextFilter(EXCEPTION_POINTERS* exception) {
    LPTOP_LEVEL_EXCEPTION_FILTER next = g_gameFilter != nullptr ? g_gameFilter : g_previousFilter;
    if (next == nullptr || next == &TopLevelFilter) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    return next(exception);
}

LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* exception) {
    if (g_handling.exchange(true)) {
        // Plantage pendant notre propre traitement : ne pas boucler.
        return EXCEPTION_CONTINUE_SEARCH;
    }
    try {
        LogException(*exception);
        save::LogCrashState();
        if (GetConfig().crashDumps) {
            WriteDump(exception);
        }
        log::Crash("Transmission au gestionnaire du jeu ({})",
                   g_gameFilter != nullptr ? sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(g_gameFilter))
                                           : std::string("aucun"));
    } catch (...) {
    }
    return ChainToNextFilter(exception);
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI HookSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter) {
    LPTOP_LEVEL_EXCEPTION_FILTER previous = g_gameFilter != nullptr ? g_gameFilter : g_previousFilter;
    g_gameFilter = filter;
    log::Info("Le jeu installe son gestionnaire d'exceptions : {} (conservé derrière le nôtre)",
              filter != nullptr ? sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(filter))
                                : std::string("aucun"));
    return previous;
}

}  // namespace

void Install(HMODULE gameModule) {
    PruneOldDumps();
    g_previousFilter = ::SetUnhandledExceptionFilter(&TopLevelFilter);
    if (!hooking::PatchImport(gameModule, "kernel32.dll", "SetUnhandledExceptionFilter",
                              &HookSetUnhandledExceptionFilter, &g_gameSetFilter)) {
        log::Warn("Interception de SetUnhandledExceptionFilter impossible : le jeu pourrait remplacer notre filtre");
    }
    log::Info("Gestionnaire de plantage installé (dumps : {}, mode {})", GetConfig().crashDumps ? "oui" : "non",
              GetConfig().fullMemoryDumps ? "complet" : "réduit");
}

void PreloadDbgHelp() noexcept {
    if (LoadDbgHelp() == nullptr) {
        log::Warn("dbghelp.dll introuvable : les fichiers de diagnostic ne pourront pas être écrits");
    }
}

void EnsureInstalled() noexcept {
    const LPTOP_LEVEL_EXCEPTION_FILTER current = ::SetUnhandledExceptionFilter(&TopLevelFilter);
    if (current == &TopLevelFilter) {
        return;
    }
    // Un autre module (pas DP.exe, dont l'appel est intercepté) a installé son filtre : on le garde
    // dans la chaîne à la place de celui du jeu s'il n'y en a pas.
    // En pratique : CSERHelper.dll, le rapporteur de plantages de Steam, installé après le démarrage.
    try {
        log::Info("Gestionnaire de plantage installé par un autre module ({}) : le nôtre est remis en tête, "
                  "toujours chaîné au gestionnaire du jeu",
                  current != nullptr ? sysinfo::FormatAddress(reinterpret_cast<std::uintptr_t>(current))
                                     : std::string("aucun"));
    } catch (...) {
    }
    if (g_gameFilter == nullptr) {
        g_gameFilter = current;
    }
}

}  // namespace dpsf::crash
