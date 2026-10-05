cmake_minimum_required(VERSION 3.21)

project(DPStabilityFix VERSION 0.2.1 LANGUAGES C CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "DPStabilityFix se compile avec MSVC (Build Tools for Visual Studio).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "DP.exe est un exécutable 32 bits : configurer en x86 (preset x86-*, ou -A Win32).")
endif()

# CRT statique : la DLL ne dépend d'aucun redistribuable Visual C++ installé.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- Dépendances téléchargées (rien de tiers non libre n'est stocké dans le dépôt) -----------------
include(FetchContent)

# D3DX9 (Microsoft, licence du SDK DirectX) : en-têtes et bibliothèque d'import, nécessaires à DPfix.
# À l'exécution, d3dx9_43.dll est celle installée avec le jeu (runtime DirectX).
FetchContent_Declare(d3dx
    URL https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX/9.29.952.8
    URL_HASH SHA256=ead0906ae8a26c18a7525da7490127a2110f7c58f18293738283e30e97c6ea4b
    DOWNLOAD_NAME microsoft.dxsdk.d3dx.9.29.952.8.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR aucun-cmakelists
)
# MinHook (BSD-2-Clause) : remplace Microsoft Detours Express (non libre) utilisé par DPfix.
FetchContent_Declare(minhook
    URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.zip
    URL_HASH SHA256=172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR aucun-cmakelists
)
FetchContent_MakeAvailable(d3dx minhook)

add_library(minhook STATIC
    "${minhook_SOURCE_DIR}/src/buffer.c"
    "${minhook_SOURCE_DIR}/src/hook.c"
    "${minhook_SOURCE_DIR}/src/trampoline.c"
    "${minhook_SOURCE_DIR}/src/hde/hde32.c"
)
target_include_directories(minhook PUBLIC "${minhook_SOURCE_DIR}/include")

add_library(d3dx9 INTERFACE)
target_include_directories(d3dx9 INTERFACE "${d3dx_SOURCE_DIR}/build/native/include")
# Toujours la bibliothèque « release » : d3dx9d_43.dll (debug) n'est pas installée chez les joueurs.
target_link_libraries(d3dx9 INTERFACE "${d3dx_SOURCE_DIR}/build/native/release/lib/x86/d3dx9.lib" dxguid)

# --- DPfix (Durante, GPL-3.0-or-later), voir third_party/dpfix/ORIGINE.md ----------------------------
# Code de 2013 compilé tel quel autant que possible : avertissements non bloquants, mode permissif.
add_library(dpfix STATIC
    third_party/dpfix/src/Detouring.cpp
    third_party/dpfix/src/Effect.cpp
    third_party/dpfix/src/FXAA.cpp
    third_party/dpfix/src/GAUSS.cpp
    third_party/dpfix/src/KeyActions.cpp
    third_party/dpfix/src/RenderstateManager.cpp
    third_party/dpfix/src/SMAA.cpp
    third_party/dpfix/src/SSAO.cpp
    third_party/dpfix/src/Settings.cpp
    third_party/dpfix/src/WindowManager.cpp
    third_party/dpfix/src/d3d9dev.cpp
    third_party/dpfix/src/d3d9int.cpp
    third_party/dpfix/src/d3dutil.cpp
    src/graphics/dpfix_bridge/DetoursShim.cpp
    src/graphics/dpfix_bridge/DpfixBridge.cpp
)
# Pas de third_party/dpfix/src dans les chemins : son d3d9.h masquerait celui du SDK Windows
# (#include <d3d9.h>). Les fichiers de DPfix s'incluent entre eux par chemin relatif ("..."), et le pont
# les inclut via "dpfix/src/...".
target_include_directories(dpfix
    PUBLIC src/graphics/dpfix_bridge
    PRIVATE third_party src
)
target_compile_features(dpfix PRIVATE cxx_std_20)
# _HAS_STD_BYTE=0 : std::byte (C++17) entre en conflit avec le byte de Windows via les « using namespace
# std » de DPfix. /FI : en-têtes standard que le compilateur de 2013 incluait implicitement.
target_compile_definitions(dpfix PRIVATE _CRT_SECURE_NO_WARNINGS _HAS_STD_BYTE=0)
target_compile_options(dpfix PRIVATE /W3 /utf-8 /EHsc /permissive /Zc:strictStrings- /wd4005
    /FIstring /FIalgorithm)
target_link_libraries(dpfix PUBLIC d3dx9 minhook winmm)

add_library(DPStabilityFix SHARED
    src/dllmain.cpp
    src/core/Config.cpp
    src/core/Hooking.cpp
    src/core/Log.cpp
    src/core/Paths.cpp
    src/core/Strings.cpp
    src/core/SystemInfo.cpp
    src/crash/CrashHandler.cpp
    src/framepacing/FrameMonitor.cpp
    src/graphics/D3D9Hooks.cpp
    src/input/ControllerHooks.cpp
    src/input/ControllerMapping.cpp
    src/patches/FpuPatches.cpp
    src/patches/SkipIntro.cpp
    src/patches/ZeroDeltaGuard.cpp
    src/proxy/X3DAudioProxy.cpp
    src/proxy/X3DAudio1_7.def
    src/save/SaveGuard.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

# Ressource de version : permet de reconnaître notre X3DAudio1_7.dll (voir launcher, ModInstaller).
configure_file(src/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

# Le jeu charge cette DLL à la place de la vraie X3DAudio1_7.dll (voir src/proxy/).
set_target_properties(DPStabilityFix PROPERTIES OUTPUT_NAME "X3DAudio1_7")

target_include_directories(DPStabilityFix PRIVATE src)
target_compile_features(DPStabilityFix PRIVATE cxx_std_20)
target_compile_definitions(DPStabilityFix PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    DPSF_VERSION="${PROJECT_VERSION}"
)
target_compile_options(DPStabilityFix PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_link_libraries(DPStabilityFix PRIVATE dpfix delayimp)
# d3dx9_43.dll n'est chargée qu'à la première utilisation par DPfix : la DLL se charge même sans le
# runtime DirectX (tests, CI) tant que DPfix n'est pas activé.
target_link_options(DPStabilityFix PRIVATE /DELAYLOAD:d3dx9_43.dll
    $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)

# Copie de la DLL à chaque édition de liens de la DLL elle-même : attachée à une autre cible, elle n'était
# pas refaite quand seule la DLL changeait (paquet et faux jeu gardaient une version périmée).
set(DPSF_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
set(DPSF_FAKE_GAME_DIR "${CMAKE_BINARY_DIR}/fake_game")
# Paquet distribuable (build/<preset>/package) : la DLL, les .ini, les shaders de DPfix et la licence. Le
# launcher (launcher/, C# Avalonia) y est ajouté par tools/build.ps1 (dotnet publish).
add_custom_command(TARGET DPStabilityFix POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DPSF_PACKAGE_DIR}" "${DPSF_FAKE_GAME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DPStabilityFix> "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DPStabilityFix> "${DPSF_FAKE_GAME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/DPStabilityFix.ini" "${DPSF_PACKAGE_DIR}"
    # DPfix intégré : réglages et shaders (dossier dpfix\ à côté de DP.exe).
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/DPfix.ini" "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/dpfix/DPfixKeys.ini" "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/third_party/dpfix/shaders" "${DPSF_PACKAGE_DIR}/dpfix"
    # GPL-3.0 : le texte de la licence accompagne les binaires distribués.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${DPSF_PACKAGE_DIR}/LICENSE.txt"
)

# Faux DP.exe (tests/fake_game) : exerce le mod sans le jeu, voir tests/run-tests.ps1.
option(DPSF_BUILD_TESTS "Compiler le faux DP.exe de test" ON)
if(DPSF_BUILD_TESTS)
    # Tests unitaires des fonctions pures (conversion de manette...), sans jeu ni matériel.
    add_executable(DPStabilityFixUnitTests tests/unit/main.cpp src/input/ControllerMapping.cpp)
    target_include_directories(DPStabilityFixUnitTests PRIVATE src)
    target_compile_features(DPStabilityFixUnitTests PRIVATE cxx_std_20)
    target_compile_definitions(DPStabilityFixUnitTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(DPStabilityFixUnitTests PRIVATE /W4 /WX /permissive- /utf-8)

    add_executable(DPStabilityFixFakeGame tests/fake_game/main.cpp)
    set_target_properties(DPStabilityFixFakeGame PROPERTIES
        OUTPUT_NAME "DP"
        RUNTIME_OUTPUT_DIRECTORY "${DPSF_FAKE_GAME_DIR}"
    )
    target_compile_features(DPStabilityFixFakeGame PRIVATE cxx_std_20)
    target_compile_definitions(DPStabilityFixFakeGame PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(DPStabilityFixFakeGame PRIVATE /W4 /WX /permissive- /utf-8)
    # Importer X3DAudio1_7.dll comme DP.exe : lien sur la bibliothèque d'import du mod.
    target_link_libraries(DPStabilityFixFakeGame PRIVATE DPStabilityFix d3d9 winmm)
    add_custom_command(TARGET DPStabilityFixFakeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/third_party/dpfix/shaders" "${DPSF_FAKE_GAME_DIR}/dpfix"
    )
endif()
