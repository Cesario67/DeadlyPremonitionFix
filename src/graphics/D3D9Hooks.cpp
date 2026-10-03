#include "graphics/D3D9Hooks.h"

#include <d3d9.h>

#include <atomic>
#include <cstdint>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"
#include "core/SystemInfo.h"
#include "crash/CrashHandler.h"
#include "framepacing/FrameMonitor.h"
#include "graphics/dpfix_bridge/DpfixBridge.h"

namespace dpsf::graphics {

namespace {

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*,
                                                   IDirect3DDevice9**);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

// Indices dans les vtables COM (ordre de déclaration de d3d9.h, IUnknown compris).
constexpr size_t kCreateDeviceIndex = 16;  // IDirect3D9::CreateDevice
constexpr size_t kResetIndex = 16;         // IDirect3DDevice9::Reset
constexpr size_t kPresentIndex = 17;       // IDirect3DDevice9::Present

Direct3DCreate9Fn g_direct3DCreate9 = nullptr;
CreateDeviceFn g_createDevice = nullptr;
ResetFn g_reset = nullptr;
PresentFn g_present = nullptr;
std::atomic<bool> g_lateInitDone{false};
HRESULT g_lastPresentError = S_OK;
bool g_localD3d9 = false;           // d3d9.dll chargé depuis le dossier du jeu (DPfix)
std::uint32_t g_resetFailures = 0;  // échecs consécutifs de Reset

void LogPresentParameters(const char* context, const D3DPRESENT_PARAMETERS& params) {
    log::Info("{} : {}x{} format {} x{} tampons, MSAA {}, swap {}, fenêtré {}, {} Hz, intervalle 0x{:X}, options 0x{:X}",
              context, params.BackBufferWidth, params.BackBufferHeight, static_cast<int>(params.BackBufferFormat),
              params.BackBufferCount, static_cast<int>(params.MultiSampleType), static_cast<int>(params.SwapEffect),
              params.Windowed ? "oui" : "non", params.FullScreen_RefreshRateInHz, params.PresentationInterval,
              params.Flags);
}

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device, const RECT* sourceRect, const RECT* destRect,
                                      HWND window, const RGNDATA* dirtyRegion) {
    frames::OnBeforePresent();
    const HRESULT result = g_present(device, sourceRect, destRect, window, dirtyRegion);
    if (frames::OnAfterPresent()) {
        crash::EnsureInstalled();
    }
    // Journaliser seulement les changements d'état (un périphérique perdu échoue à chaque image).
    if (result != g_lastPresentError) {
        if (FAILED(result)) {
            log::Warn("Present échoue : 0x{:08X}", static_cast<unsigned long>(result));
        } else if (FAILED(g_lastPresentError)) {
            log::Info("Present fonctionne de nouveau");
        }
        g_lastPresentError = result;
    }
    return result;
}

// DPfix (vérifié dans les sources 0.9, RenderstateManager.cpp) garde des références vers des surfaces
// du jeu (mainSurface, depthSurface) d'un appel de rendu au Present suivant, et ne les libère pas dans
// son Reset. Si le périphérique est perdu en cours d'image (alt-tab en plein écran), le jeu enchaîne
// les Reset sans plus présenter : Direct3D refuse chaque Reset (D3DERR_INVALIDCALL) et le jeu reste
// bloqué. Un Present (qui échoue, le périphérique étant perdu) fait relâcher ces références à DPfix.
HRESULT ReleaseDpfixFrameReferencesAndRetry(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    const HRESULT presentResult = g_present(device, nullptr, nullptr, nullptr, nullptr);
    const HRESULT result = g_reset(device, params);
    if (g_resetFailures == 0 || SUCCEEDED(result)) {
        log::Info("Contournement DPfix : Present -> 0x{:08X}, nouveau Reset -> 0x{:08X}",
                  static_cast<unsigned long>(presentResult), static_cast<unsigned long>(result));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    // Le jeu retente Reset toutes les 50 ms tant qu'il échoue : on ne journalise que le premier échec,
    // un échec sur 100, et le retour à la normale.
    if (params != nullptr && g_resetFailures == 0) {
        LogPresentParameters("Reset du périphérique", *params);
    }
    HRESULT result = g_reset(device, params);
    if (result == D3DERR_INVALIDCALL && g_localD3d9 && GetConfig().dpfixResetWorkaround) {
        result = ReleaseDpfixFrameReferencesAndRetry(device, params);
    }

    if (SUCCEEDED(result)) {
        if (g_resetFailures > 0) {
            log::Info("Reset réussi après {} échec(s)", g_resetFailures);
        } else {
            log::Info("Reset -> 0x{:08X}", static_cast<unsigned long>(result));
        }
        g_resetFailures = 0;
    } else {
        ++g_resetFailures;
        if (g_resetFailures == 1 || g_resetFailures % 100 == 0) {
            log::Warn("Reset -> 0x{:08X} ({} échec(s) consécutif(s))", static_cast<unsigned long>(result),
                      g_resetFailures);
        }
    }
    frames::OnDeviceReset();
    return result;
}

void PatchDevice(IDirect3DDevice9* device) {
    // Toutes les instances d'une même classe partagent la vtable : une seule modification suffit.
    // Si un objet d'une autre classe apparaît, on ne l'intercepte pas (l'original mémorisé serait faux).
    if (g_present != nullptr) {
        void** vtable = *reinterpret_cast<void***>(device);
        if (vtable[kPresentIndex] != reinterpret_cast<void*>(&HookPresent)) {
            log::Warn("Nouveau périphérique avec une vtable différente : non intercepté");
        }
        return;
    }
    if (!hooking::PatchVtable(device, kPresentIndex, &HookPresent, &g_present) ||
        !hooking::PatchVtable(device, kResetIndex, &HookReset, &g_reset)) {
        log::Error("Interception de Present/Reset impossible : pas de mesure de cadence");
        return;
    }
    log::Info("Present intercepté (implémentation : {})",
              WideToUtf8(sysinfo::ModulePathOf(reinterpret_cast<const void*>(g_present))));
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9* direct3d, UINT adapter, D3DDEVTYPE deviceType, HWND window,
                                           DWORD behaviorFlags, D3DPRESENT_PARAMETERS* params,
                                           IDirect3DDevice9** device) {
    if (params != nullptr) {
        LogPresentParameters("CreateDevice demandé", *params);
    }
    // DP.exe demande 0x44 (HARDWARE_VERTEXPROCESSING | MULTITHREADED), sans FPU_PRESERVE.
    if (GetConfig().forceFpuPreserve && (behaviorFlags & D3DCREATE_FPU_PRESERVE) == 0) {
        behaviorFlags |= D3DCREATE_FPU_PRESERVE;
        log::Info("D3DCREATE_FPU_PRESERVE ajouté : le x87 du thread de rendu garde sa précision");
    }
    const HRESULT result = g_createDevice(direct3d, adapter, deviceType, window, behaviorFlags, params, device);
    log::Info("CreateDevice (adaptateur {}, type {}, comportement 0x{:08X}) -> 0x{:08X}", adapter,
              static_cast<int>(deviceType), behaviorFlags, static_cast<unsigned long>(result));
    if (SUCCEEDED(result) && device != nullptr && *device != nullptr) {
        try {
            PatchDevice(*device);
        } catch (...) {
        }
    }
    return result;
}

// Premier appel du jeu à Direct3D : le chargement des DLL est terminé, on peut faire ce qui est
// interdit dans DllMain (LoadLibrary...).
void LateInit() {
    if (g_lateInitDone.exchange(true)) {
        return;
    }
    frames::LateInit();
    crash::PreloadDbgHelp();

    // L'IAT peut pointer vers un shim de compatibilité de Windows (apphelp.dll) plutôt que vers
    // d3d9.dll : on identifie donc DPfix par le module « d3d9.dll » réellement chargé.
    const std::wstring caller = sysinfo::ModulePathOf(reinterpret_cast<const void*>(g_direct3DCreate9));
    const HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    const std::wstring provider = d3d9 != nullptr ? sysinfo::ModulePathOf(d3d9) : std::wstring(L"(non chargé)");
    const std::wstring& gameDir = GetPaths().gameDir;
    const bool local = provider.size() > gameDir.size() && EqualsIgnoreCase(provider.substr(0, gameDir.size()), gameDir);
    g_localD3d9 = local;
    log::Info("d3d9.dll chargé : {}{}", WideToUtf8(provider),
              local ? " (DLL locale : DPfix ou autre wrapper)" : " (DLL système, DPfix absent)");
    if (!EqualsIgnoreCase(caller, provider)) {
        log::Info("Direct3DCreate9 du jeu passe d'abord par : {} (shim de compatibilité Windows ou autre "
                  "intercepteur)",
                  WideToUtf8(caller));
    }
}

IDirect3D9* WINAPI HookDirect3DCreate9(UINT sdkVersion) {
    try {
        LateInit();
    } catch (...) {
    }
    IDirect3D9* direct3d = g_direct3DCreate9(sdkVersion);
    log::Info("Direct3DCreate9({}) -> {}", sdkVersion, direct3d != nullptr ? "ok" : "échec");
    if (direct3d != nullptr && GetConfig().integratedDpfix) {
        if (g_localD3d9) {
            log::Warn("Un d3d9.dll externe (DPfix d'origine ?) est présent : DPfix intégré désactivé pour ne pas "
                      "traiter l'image deux fois. Retirer d3d9.dll du dossier du jeu pour utiliser la version "
                      "intégrée et corrigée.");
        } else if (dpfix::Initialize()) {
            direct3d = dpfix::Wrap(direct3d);
            log::Info("DPfix intégré actif");
        }
    }
    if (direct3d != nullptr && g_createDevice == nullptr) {
        if (!hooking::PatchVtable(direct3d, kCreateDeviceIndex, &HookCreateDevice, &g_createDevice)) {
            log::Error("Interception de CreateDevice impossible");
        }
    }
    return direct3d;
}

}  // namespace

void Install(HMODULE gameModule) {
    if (!hooking::PatchImport(gameModule, "d3d9.dll", "Direct3DCreate9", &HookDirect3DCreate9, &g_direct3DCreate9)) {
        log::Error("Interception de Direct3DCreate9 impossible : pas de mesure de cadence");
    }
}

}  // namespace dpsf::graphics
