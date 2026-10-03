cmake_minimum_required(VERSION 3.21)

project(DPStabilityFix VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "DPStabilityFix se compile avec MSVC (Build Tools for Visual Studio).")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "DP.exe est un exécutable 32 bits : configurer en x86 (preset x86-*, ou -A Win32).")
endif()

# CRT statique : la DLL ne dépend d'aucun redistribuable Visual C++ installé.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    src/proxy/X3DAudioProxy.cpp
    src/proxy/X3DAudio1_7.def
    src/save/SaveGuard.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

# Ressource de version : permet de reconnaître notre X3DAudio1_7.dll (voir tools/deploy.ps1).
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
target_link_options(DPStabilityFix PRIVATE $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)

# Installeur : sélection de DP.exe, patch 4 Go, copie de la DLL et du .ini.
add_executable(DPStabilityFixSetup WIN32
    setup/main.cpp
    setup/ExePatcher.cpp
    setup/ModInstaller.cpp
)
target_compile_features(DPStabilityFixSetup PRIVATE cxx_std_20)
target_compile_definitions(DPStabilityFixSetup PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(DPStabilityFixSetup PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
target_link_libraries(DPStabilityFixSetup PRIVATE comdlg32 shell32 version)

# Paquet distribuable : l'installeur, la DLL et le .ini dans un même dossier.
set(DPSF_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
add_custom_command(TARGET DPStabilityFixSetup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DPStabilityFixSetup> "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DPStabilityFix> "${DPSF_PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/DPStabilityFix.ini" "${DPSF_PACKAGE_DIR}"
    # GPL-3.0 : le texte de la licence accompagne les binaires distribués.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${DPSF_PACKAGE_DIR}/LICENSE.txt"
)
add_dependencies(DPStabilityFixSetup DPStabilityFix)

# Faux DP.exe (tests/fake_game) : exerce le mod sans le jeu, voir tests/run-tests.ps1.
option(DPSF_BUILD_TESTS "Compiler le faux DP.exe de test" ON)
if(DPSF_BUILD_TESTS)
    set(DPSF_FAKE_GAME_DIR "${CMAKE_BINARY_DIR}/fake_game")
    add_executable(DPStabilityFixFakeGame tests/fake_game/main.cpp)
    set_target_properties(DPStabilityFixFakeGame PROPERTIES
        OUTPUT_NAME "DP"
        RUNTIME_OUTPUT_DIRECTORY "${DPSF_FAKE_GAME_DIR}"
    )
    target_compile_features(DPStabilityFixFakeGame PRIVATE cxx_std_20)
    target_compile_definitions(DPStabilityFixFakeGame PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(DPStabilityFixFakeGame PRIVATE /W4 /WX /permissive- /utf-8)
    # Importer X3DAudio1_7.dll comme DP.exe : lien sur la bibliothèque d'import du mod.
    target_link_libraries(DPStabilityFixFakeGame PRIVATE DPStabilityFix d3d9)
    add_custom_command(TARGET DPStabilityFixFakeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DPStabilityFix> "${DPSF_FAKE_GAME_DIR}"
    )
endif()
