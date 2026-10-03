// Faux DP.exe pour tester DPStabilityFix sans le jeu.
//
// Il importe X3DAudio1_7.dll, d3d9.dll et les mêmes fonctions de kernel32 que DP.exe : le mod s'y
// charge et s'y accroche exactement comme dans le jeu. Chaque scénario (premier argument) reproduit un
// comportement à vérifier ; tests/run-tests.ps1 contrôle ensuite les fichiers et le journal produits.

#include <windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <string>

// Prototype de la SDK DirectX juin 2010 (X3DAudio.h) : convention cdecl, handle opaque de 20 octets.
extern "C" __declspec(dllimport) void __cdecl X3DAudioInitialize(UINT32 speakerChannelMask, float speedOfSound,
                                                                  BYTE instance[20]);

namespace {

// Même chemin que DP.exe (chaîne à 0x76F554), avec une barre oblique.
constexpr const char* kSavePath = "savedata/dp.sav";

int Fail(const char* message) {
    std::fprintf(stderr, "ECHEC : %s (erreur %lu)\n", message, GetLastError());
    return 1;
}

bool WriteSave(const char* content, bool closeAfter, bool flushMidway) {
    const HANDLE file = CreateFileA(kSavePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD length = static_cast<DWORD>(std::strlen(content));
    const DWORD half = length / 2;
    DWORD written = 0;
    if (!WriteFile(file, content, half, &written, nullptr)) {
        return false;
    }
    if (flushMidway && !FlushFileBuffers(file)) {
        return false;
    }
    if (!WriteFile(file, content + half, length - half, &written, nullptr)) {
        return false;
    }
    return !closeAfter || CloseHandle(file);
}

std::string ReadSave() {
    const HANDLE file = CreateFileA(kSavePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return "<absent>";
    }
    char buffer[4096]{};
    DWORD read = 0;
    ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
    CloseHandle(file);
    return std::string(buffer, read);
}

int ScenarioAudio() {
    BYTE handle[20]{};
    constexpr UINT32 kStereo = 0x1 | 0x2;  // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT (ksmedia.h)
    X3DAudioInitialize(kStereo, 343.5f, handle);
    for (const BYTE value : handle) {
        if (value != 0) {
            std::printf("X3DAudioInitialize transmis a la vraie DLL\n");
            return 0;
        }
    }
    return Fail("X3DAudioInitialize n'a pas rempli le handle");
}

int ScenarioSave() {
    CreateDirectoryA("savedata", nullptr);
    if (!WriteSave("partie-v1", true, false)) {
        return Fail("ecriture v1");
    }
    if (ReadSave() != "partie-v1") {
        return Fail("relecture v1");
    }
    if (!WriteSave("partie-v2-plus-longue", true, true)) {
        return Fail("ecriture v2");
    }
    if (ReadSave() != "partie-v2-plus-longue") {
        return Fail("relecture v2");
    }
    std::printf("Sauvegardes ecrites et relues\n");
    return 0;
}

// Plantage brutal pendant l'écriture : aucune notification au mod (comme un crash fatal du jeu).
int ScenarioSaveCrash() {
    CreateDirectoryA("savedata", nullptr);
    if (!WriteSave("partie-stable", true, false)) {
        return Fail("ecriture initiale");
    }
    if (!WriteSave("CORROMPU-INCOMPLET", false, false)) {
        return Fail("ecriture interrompue");
    }
    TerminateProcess(GetCurrentProcess(), 3);
    return 0;
}

// Fermeture normale du jeu sans avoir fermé le fichier de sauvegarde.
int ScenarioSaveExit() {
    CreateDirectoryA("savedata", nullptr);
    if (!WriteSave("partie-avant-sortie", true, false)) {
        return Fail("ecriture initiale");
    }
    if (!WriteSave("partie-ecrite-a-la-sortie", false, false)) {
        return Fail("ecriture sans fermeture");
    }
    ExitProcess(0);
}

int ScenarioSaveDelete() {
    CreateDirectoryA("savedata", nullptr);
    if (!WriteSave("partie-a-supprimer", true, false)) {
        return Fail("ecriture initiale");
    }
    if (!DeleteFileA(kSavePath)) {
        return Fail("suppression");
    }
    return 0;
}

LONG WINAPI GameFilter(EXCEPTION_POINTERS*) {
    const HANDLE marker =
        CreateFileA("gamefilter.txt", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CloseHandle(marker);
    return EXCEPTION_EXECUTE_HANDLER;  // termine le processus sans boîte de dialogue
}

int ScenarioCrash() {
    SetUnhandledExceptionFilter(&GameFilter);
    volatile int* invalid = reinterpret_cast<volatile int*>(static_cast<uintptr_t>(0x10));
    *invalid = 42;
    return 0;
}

// Reproduit DP.exe 0x401F50 : microsecondes = QPC absolu * (float)(1e6 / fréquence), calcul x87.
// Le résultat dépend de la précision du x87 du thread appelant.
long long GameStyleMicroseconds(long long counterOffset, float factor) {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    long long value = counter.QuadPart + counterOffset;
    long long result = 0;
    __asm {
        fild value
        fld factor
        fmulp st(1), st
        fistp result
    }
    return result;
}

// Plus petit écart non nul entre deux valeurs successives du temps « façon DP.exe », sur un PC
// supposé allumé depuis `uptimeSeconds` de plus.
double MeasureGameTimeStepMs(double uptimeSeconds) {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    const auto factor = static_cast<float>(1e6 / static_cast<double>(frequency.QuadPart));
    const auto offset = static_cast<long long>(uptimeSeconds * static_cast<double>(frequency.QuadPart));
    const ULONGLONG end = GetTickCount64() + 400;
    long long previous = GameStyleMicroseconds(offset, factor);
    long long smallest = 0;
    while (GetTickCount64() < end) {
        const long long current = GameStyleMicroseconds(offset, factor);
        if (current != previous) {
            const long long step = current - previous;
            if (step > 0 && (smallest == 0 || step < smallest)) {
                smallest = step;
            }
            previous = current;
        }
    }
    return static_cast<double>(smallest) / 1000.0;
}

int X87PrecisionBits() {
    unsigned short control = 0;
    __asm fnstcw control
    const int field = (control >> 8) & 3;
    return field == 0 ? 24 : field == 2 ? 53 : field == 3 ? 64 : 0;
}

// Comme DP.exe : l'écran de chargement présente depuis un thread secondaire, puis le thread
// principal prend le relais.
DWORD WINAPI LoadingScreenThread(LPVOID parameter) {
    auto* device = static_cast<IDirect3DDevice9*>(parameter);
    for (int frame = 0; frame < 10; ++frame) {
        device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, frame * 20), 1.0f, 0);
        device->Present(nullptr, nullptr, nullptr, nullptr);
        Sleep(16);
    }
    return 0;
}

int ScenarioFrames() {
    WNDCLASSA windowClass{};
    windowClass.lpfnWndProc = DefWindowProcA;
    windowClass.hInstance = GetModuleHandleA(nullptr);
    windowClass.lpszClassName = "DPStabilityFixTest";
    RegisterClassA(&windowClass);
    const HWND window = CreateWindowA("DPStabilityFixTest", "DPStabilityFix test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240,
                                      nullptr, nullptr, windowClass.hInstance, nullptr);
    if (window == nullptr) {
        return Fail("creation de la fenetre");
    }
    IDirect3D9* direct3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (direct3d == nullptr) {
        return Fail("Direct3DCreate9");
    }
    D3DPRESENT_PARAMETERS params{};
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.BackBufferFormat = D3DFMT_UNKNOWN;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9* device = nullptr;
    // Mêmes options que DP.exe (0x006CC569 : 0x44), donc sans D3DCREATE_FPU_PRESERVE.
    if (FAILED(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &params,
                                      &device))) {
        return Fail("CreateDevice");
    }

    // Résolution du temps du jeu après CreateDevice, sur un PC allumé depuis 7 jours.
    constexpr double kSevenDays = 7.0 * 24.0 * 3600.0;
    const double stepMs = MeasureGameTimeStepMs(kSevenDays);
    FILE* report = nullptr;
    if (fopen_s(&report, "timer-precision.txt", "w") == 0 && report != nullptr) {
        std::fprintf(report, "x87=%d step_ms=%.3f\n", X87PrecisionBits(), stepMs);
        std::fclose(report);
    }
    const HANDLE loading = CreateThread(nullptr, 0, &LoadingScreenThread, device, 0, nullptr);
    if (loading != nullptr) {
        WaitForSingleObject(loading, INFINITE);
        CloseHandle(loading);
    }

    // Rythme « à la DP.exe » : une image puis Sleep, environ 5 secondes.
    for (int frame = 0; frame < 150; ++frame) {
        device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(frame, 0, 0), 1.0f, 0);
        device->Present(nullptr, nullptr, nullptr, nullptr);
        Sleep(30);
    }
    device->Release();
    direct3d->Release();
    DestroyWindow(window);
    std::printf("150 images presentees\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // Le mod travaille par rapport au dossier de DP.exe, comme le jeu.
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    *std::strrchr(exePath, '\\') = '\0';
    SetCurrentDirectoryA(exePath);

    // DP.exe importe DeleteFileA : on l'importe aussi pour que le mod puisse l'intercepter.
    DeleteFileA("dpsf-fichier-inexistant.tmp");

    const std::string scenario = argc > 1 ? argv[1] : "";
    if (scenario == "audio") return ScenarioAudio();
    if (scenario == "save") return ScenarioSave();
    if (scenario == "save-crash") return ScenarioSaveCrash();
    if (scenario == "save-exit") return ScenarioSaveExit();
    if (scenario == "save-delete") return ScenarioSaveDelete();
    if (scenario == "crash") return ScenarioCrash();
    if (scenario == "frames") return ScenarioFrames();
    std::fprintf(stderr, "Scenarios : audio | save | save-crash | save-exit | save-delete | crash | frames\n");
    return 2;
}
