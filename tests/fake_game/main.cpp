// Faux DP.exe pour tester DPStabilityFix sans le jeu.
//
// Il importe X3DAudio1_7.dll, d3d9.dll et les mêmes fonctions de kernel32 que DP.exe : le mod s'y
// charge et s'y accroche exactement comme dans le jeu. Chaque scénario (premier argument) reproduit un
// comportement à vérifier ; tests/run-tests.ps1 contrôle ensuite les fichiers et le journal produits.

#include <windows.h>
#include <d3d9.h>
#include <mmsystem.h>
#include <float.h>

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

long long g_timeOffset = 0;
float g_timeFactor = 0.0f;

int X87PrecisionBits() {
    unsigned short control = 0;
    __asm fnstcw control
    const int field = (control >> 8) & 3;
    return field == 0 ? 24 : field == 2 ? 53 : field == 3 ? 64 : 0;
}

int g_aimInsideBits = 0;

}  // namespace

// Équivalents des fonctions de DP.exe que le mod intercepte (patches/FpuPatches) : DP.exe n'exportant
// rien, le mod cherche ces exports quand l'exécutable n'est pas la version 1.01b analysée.
extern "C" __declspec(dllexport) __declspec(noinline) long long __cdecl DpsfTestGameMicroseconds() {
    return GameStyleMicroseconds(g_timeOffset, g_timeFactor);
}

// Copie de l'instruction de DP.exe (0x643F2D) qui choisit l'étape de départ du lancement : 0xB3 = logos et
// introduction, 0 = écran titre (patches/SkipIntro).
extern "C" __declspec(dllexport) const unsigned char DpsfTestIntroInstruction[10] = {0xC7, 0x05, 0xD8, 0x36, 0x47,
                                                                                    0x01, 0xB3, 0x00, 0x00, 0x00};

// Comme la gestion de la caméra de visée (0x53B8B0) : note la précision du x87 pendant son exécution.
extern "C" __declspec(dllexport) __declspec(noinline) void __cdecl DpsfTestAimHandler() {
    g_aimInsideBits = X87PrecisionBits();
}

namespace {

// Plus petit écart non nul entre deux valeurs successives du temps « façon DP.exe », sur un PC
// supposé allumé depuis `uptimeSeconds` de plus. Appel par l'export, comme le jeu appelle sa fonction.
double MeasureGameTimeStepMs(double uptimeSeconds) {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    g_timeFactor = static_cast<float>(1e6 / static_cast<double>(frequency.QuadPart));
    g_timeOffset = static_cast<long long>(uptimeSeconds * static_cast<double>(frequency.QuadPart));
    using TimeFn = long long(__cdecl*)();
    const auto gameTime = reinterpret_cast<TimeFn>(GetProcAddress(GetModuleHandleA(nullptr), "DpsfTestGameMicroseconds"));
    const ULONGLONG end = GetTickCount64() + 400;
    long long previous = gameTime();
    long long smallest = 0;
    while (GetTickCount64() < end) {
        const long long current = gameTime();
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

// Appelle la « gestion de la visée » avec le x87 en double précision (cas où la visée se bloque) et
// renvoie la précision vue à l'intérieur ; `after` reçoit celle rétablie au retour.
int AimPrecisionBits(int& after) {
    unsigned int previous = 0;
    _controlfp_s(&previous, 0, 0);
    unsigned int ignored = 0;
    _controlfp_s(&ignored, _PC_53, _MCW_PC);
    using AimFn = void(__cdecl*)();
    const auto aim = reinterpret_cast<AimFn>(GetProcAddress(GetModuleHandleA(nullptr), "DpsfTestAimHandler"));
    g_aimInsideBits = 0;
    aim();
    after = X87PrecisionBits();
    _controlfp_s(&ignored, previous & _MCW_PC, _MCW_PC);
    return g_aimInsideBits;
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
    const int ambientBits = X87PrecisionBits();
    int aimAfterBits = 0;
    const int aimInsideBits = AimPrecisionBits(aimAfterBits);
    FILE* report = nullptr;
    if (fopen_s(&report, "timer-precision.txt", "w") == 0 && report != nullptr) {
        std::fprintf(report, "x87=%d step_ms=%.3f aim_inside=%d aim_after=%d\n", ambientBits, stepMs, aimInsideBits,
                     aimAfterBits);
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

// Reproduit le blocage après alt-tab avec DPfix : périphérique à 2 tampons comme DP.exe, changements
// de cible de rendu (DPfix garde alors une référence à la cible précédente), puis Reset. Le résultat
// est écrit dans reset-result.txt.
// Le scénario demande le plein écran comme DP.exe : sans DPfix réglé en fenêtré ou sans bordure, il
// changerait réellement la résolution de l'écran. On vérifie donc la configuration avant de commencer.
bool DpfixWindowedConfigured() {
    if (GetFileAttributesA("dpfix\\SMAA.fx") == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    FILE* ini = nullptr;
    if (fopen_s(&ini, "DPfix.ini", "r") != 0 || ini == nullptr) {
        return false;
    }
    bool windowed = false;
    char line[256];
    while (std::fgets(line, sizeof(line), ini) != nullptr) {
        if (std::strncmp(line, "forceWindowed 1", 15) == 0 || std::strncmp(line, "borderlessFullscreen 1", 22) == 0) {
            windowed = true;
        }
    }
    std::fclose(ini);
    return windowed;
}

struct RenderJob {
    IDirect3DDevice9* device;
    IDirect3DTexture9* renderTexture;
};

// DP.exe 1.01b dessine depuis un autre thread que celui qui possède sa fenêtre, et appelle SetViewport
// (qui déclenche l'application du mode sans bordure par DPfix).
DWORD WINAPI RenderFramesThread(LPVOID parameter) {
    const auto* job = static_cast<const RenderJob*>(parameter);
    IDirect3DDevice9* device = job->device;
    const D3DVIEWPORT9 viewport{0, 0, 1280, 720, 0.0f, 1.0f};
    for (int frame = 0; frame < 20; ++frame) {
        IDirect3DSurface9* backBuffer = nullptr;
        device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        device->SetRenderTarget(0, backBuffer);
        device->SetRenderTarget(0, backBuffer);
        backBuffer->Release();
        device->SetViewport(&viewport);
        device->SetTexture(0, job->renderTexture);
        device->SetTexture(5, job->renderTexture);
        device->SetTexture(0, nullptr);
        device->SetTexture(5, nullptr);
        device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, frame * 10, 0), 1.0f, 0);
        device->Present(nullptr, nullptr, nullptr, nullptr);
    }
    return 0;
}

int ScenarioDpfixReset() {
    if (!DpfixWindowedConfigured()) {
        return Fail("DPfix doit etre configure en fenetre (forceWindowed ou borderlessFullscreen)");
    }
    WNDCLASSA windowClass{};
    windowClass.lpfnWndProc = DefWindowProcA;
    windowClass.hInstance = GetModuleHandleA(nullptr);
    windowClass.lpszClassName = "DPStabilityFixResetTest";
    RegisterClassA(&windowClass);
    const HWND window = CreateWindowA("DPStabilityFixResetTest", "DPStabilityFix reset", WS_OVERLAPPEDWINDOW, 0, 0,
                                      320, 240, nullptr, nullptr, windowClass.hInstance, nullptr);
    IDirect3D9* direct3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (window == nullptr || direct3d == nullptr) {
        return Fail("fenetre ou Direct3DCreate9");
    }
    // Paramètres relevés dans le journal de DP.exe 1.01b : plein écran 1280x720, 2 tampons, FLIP, 59 Hz
    // à la création puis 60 Hz au Reset. DPfix les convertit en mode fenêtré.
    D3DPRESENT_PARAMETERS params{};
    params.BackBufferWidth = 1280;
    params.BackBufferHeight = 720;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 2;
    params.SwapEffect = D3DSWAPEFFECT_FLIP;
    params.hDeviceWindow = window;
    params.Windowed = FALSE;
    params.FullScreen_RefreshRateInHz = 59;
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    D3DPRESENT_PARAMETERS resetParams = params;
    resetParams.FullScreen_RefreshRateInHz = 60;
    IDirect3DDevice9* device = nullptr;
    if (FAILED(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &params,
                                      &device))) {
        return Fail("CreateDevice");
    }
    // Comme l'écran de chargement du jeu : une cible de rendu (mémoire vidéo) affichée comme texture.
    IDirect3DTexture9* renderTexture = nullptr;
    if (FAILED(device->CreateTexture(64, 64, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                     &renderTexture, nullptr))) {
        return Fail("CreateTexture");
    }
    RenderJob job{device, renderTexture};
    const HANDLE renderThread = CreateThread(nullptr, 0, &RenderFramesThread, &job, 0, nullptr);
    if (renderThread == nullptr) {
        return Fail("thread de rendu");
    }
    // Comme un vrai jeu, le thread de la fenêtre traite ses messages pendant que le rendu tourne
    // (DPfix modifie la fenêtre depuis le thread de rendu, ce qui envoie des messages à ce thread).
    while (MsgWaitForMultipleObjects(1, &renderThread, FALSE, 30000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) {
        MSG message{};
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    CloseHandle(renderThread);
    // Le jeu libère ses ressources en mémoire vidéo avant Reset, comme l'exige Direct3D 9.
    renderTexture->Release();
    const HRESULT result = device->Reset(&resetParams);
    FILE* report = nullptr;
    if (fopen_s(&report, "reset-result.txt", "w") == 0 && report != nullptr) {
        std::fprintf(report, "reset=0x%08lX\n", static_cast<unsigned long>(result));
        std::fclose(report);
    }
    device->Release();
    direct3d->Release();
    DestroyWindow(window);
    return SUCCEEDED(result) ? 0 : 1;
}

// Interroge les manettes 0 à 6 à chaque « image » comme DP.exe, pendant plus de 20 s (période de la
// réénumération de WinMM), et note l'appel le plus long dans joy-polling.txt.
int ScenarioJoyPolling() {
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    const ULONGLONG end = GetTickCount64() + 24000;
    double maxMs = 0.0;
    while (GetTickCount64() < end) {
        for (UINT id = 0; id <= 6; ++id) {
            JOYINFOEX state{};
            state.dwSize = sizeof(state);
            state.dwFlags = JOY_RETURNALL;
            LARGE_INTEGER before{};
            LARGE_INTEGER after{};
            QueryPerformanceCounter(&before);
            joyGetPosEx(id, &state);
            QueryPerformanceCounter(&after);
            const double ms =
                static_cast<double>(after.QuadPart - before.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
            if (ms > maxMs) {
                maxMs = ms;
            }
        }
        Sleep(16);
    }
    FILE* report = nullptr;
    if (fopen_s(&report, "joy-polling.txt", "w") != 0 || report == nullptr) {
        return Fail("joy-polling.txt");
    }
    std::fprintf(report, "max_ms=%.1f\n", maxMs);
    std::fclose(report);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // Le mod travaille par rapport au dossier de DP.exe, comme le jeu.
    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    *std::strrchr(exePath, '\\') = '\0';
    SetCurrentDirectoryA(exePath);

    // DP.exe importe DeleteFileA et winmm!joyGetPosEx : on les importe aussi pour que le mod puisse les
    // intercepter (sans manette branchée, joyGetPosEx échoue simplement).
    DeleteFileA("dpsf-fichier-inexistant.tmp");
    JOYINFOEX joystick{};
    joystick.dwSize = sizeof(joystick);
    joystick.dwFlags = JOY_RETURNALL;
    joyGetPosEx(0, &joystick);

    const std::string scenario = argc > 1 ? argv[1] : "";
    if (scenario == "audio") return ScenarioAudio();
    if (scenario == "save") return ScenarioSave();
    if (scenario == "save-crash") return ScenarioSaveCrash();
    if (scenario == "save-exit") return ScenarioSaveExit();
    if (scenario == "save-delete") return ScenarioSaveDelete();
    if (scenario == "crash") return ScenarioCrash();
    if (scenario == "frames") return ScenarioFrames();
    if (scenario == "dpfix-reset") return ScenarioDpfixReset();
    if (scenario == "joy-polling") return ScenarioJoyPolling();
    if (scenario == "joy-malformed") {
        // Comme le second appel de DP.exe : JOYINFOEX non initialisée (valeurs relevées en jeu).
        JOYINFOEX garbage{};
        garbage.dwSize = 1836434513;
        garbage.dwFlags = 0x1AF9F4;
        const MMRESULT bigSize = joyGetPosEx(0, &garbage);
        JOYINFOEX rawFlags{};
        rawFlags.dwSize = sizeof(rawFlags);
        rawFlags.dwFlags = JOY_RETURNALL | JOY_RETURNRAWDATA | JOY_CAL_READ4;
        const MMRESULT badFlags = joyGetPosEx(0, &rawFlags);
        FILE* report = nullptr;
        if (fopen_s(&report, "joy-malformed.txt", "w") != 0 || report == nullptr) {
            return Fail("joy-malformed.txt");
        }
        std::fprintf(report, "size=%u flags=%u\n", bigSize, badFlags);
        std::fclose(report);
        return 0;
    }
    if (scenario == "intro") {
        // Étape de départ lue après l'initialisation du mod (DllMain a déjà tourné).
        const volatile unsigned char* instruction = DpsfTestIntroInstruction;
        FILE* report = nullptr;
        if (fopen_s(&report, "intro.txt", "w") != 0 || report == nullptr) {
            return Fail("intro.txt");
        }
        std::fprintf(report, "start=0x%02X\n", instruction[6]);
        std::fclose(report);
        return 0;
    }
    if (scenario == "idle") {
        // Jeu « en cours d'exécution » pour les tests du launcher (refus d'installer pendant une partie).
        Sleep(20000);
        return 0;
    }
    std::fprintf(stderr, "Scenarios : audio | save | save-crash | save-exit | save-delete | crash | frames\n");
    return 2;
}
