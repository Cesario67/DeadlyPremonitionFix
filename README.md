# DPStabilityFix

**English** | [Français](README.fr.md)

A mod for **Deadly Premonition: The Director's Cut** (PC, Steam version 1.01b) that aims to fix every
known problem of the PC port.

Goals, by priority:

1. **Never lose a save again** because of a crash (the game uses a single file, `savedata\dp.sav`).
2. **Understand, then fix the crashes** (notably episode 2, chapter 9).
3. **Reduce stutter.**
4. **Graphics**: it integrates Durante's [DPfix](https://github.com/PeterTh/dpfix) (resolution, SMAA
   anti-aliasing, SSAO, depth of field, borderless fullscreen...), built from its 0.9 sources, with many
   bugs fixed (see below).

## Current status: version 0.2

| Feature | Status | Verified in game |
|---|---|---|
| Protected saves: atomic writes, backups, recovery after a crash | **Active** | Not yet (no save made yet); verified by tests |
| Diagnostic file (.dmp) and detailed report on every crash | **Active** | Yes |
| Frame rate, wait (`Sleep`) and memory measurements | **Active** | Yes |
| Windows timer resolution set to 1 ms | **Active**, can be turned off | Applied; effect on stutter to be confirmed |
| Game calculations in double precision (precise time, smooth camera, see [the analysis](docs/analyse-dp-exe.md), in French) | **Active**, can be turned off | Yes: in single precision, the camera stutters |
| Restricted aiming (the reticle reaches the screen edge but the camera stops following): aim camera code run in single precision, mechanism discovered by [ZachFix](https://github.com/h714je/ZachFix) | **Active**, can be turned off | Not yet; verified by tests |
| Frame limiter at 60 FPS (at 120 FPS the game looks sped up) | **Active**, adjustable | Yes |
| Logos and intro skipped at launch (a well-known community edit, done in memory) | **Active**, can be turned off | Yes |
| 4 GB patch (`LARGE_ADDRESS_AWARE`) | Applied by the launcher during installation | Yes (4 GB of address space) |
| Launcher (installation, settings, saves, launching), in English and French | **New** | Opening and game detection: yes. Installation and settings: tests only |
| Integrated, fixed DPfix | **Active** (settings in `DPfix.ini`), inactive if an original DPfix (`d3d9.dll`) is present | 1080p borderless launch: yes. Alt-tab, SMAA, SSAO: not yet |
| PlayStation controller (DualSense, DualShock 4) presented to the game as an Xbox 360 controller: the camera no longer spins on its own, buttons in the right order | **Active**, can be turned off | Yes (DualSense over Bluetooth) |
| Controller sometimes unresponsive: the game also reads the controller with an uninitialized structure that Windows may accept (frozen sticks, released buttons) | **Active** | Yes |
| ~65 ms hitch every 20 s: Windows blocks a controller read; the mod reads the controllers in the background | **Active**, can be turned off | Cause measured in game; fix not verified in game yet |
| Targeted fixes for the game's crashes | Coming, based on the diagnostics collected | |

### DPfix: bugs fixed

The published DPfix code is version 0.9, older than game version 1.01b. The following fixes were found
in game or by reading the code, and each one is reproduced by a test (details in
[third_party/dpfix/ORIGINE.md](third_party/dpfix/ORIGINE.md), in French):

| Symptom | Cause |
|---|---|
| Game frozen at its first display mode change, or after an alt-tab in fullscreen | References never released (`QueryInterface` on every displayed texture, surfaces kept during `Reset`); this was also a **continuous memory leak** |
| Crash at launch in borderless or windowed mode | The 59 Hz refresh rate requested by the game kept in windowed mode |
| Borderless mode reapplied every frame, keyboard shortcuts inactive | Window looked up with `GetActiveWindow()` from the render thread |
| Game stuck at startup if `DPfix.ini` is missing | Infinite read loop |
| Crash with FXAA | The non-free shader is absent: SMAA is used instead |
| Possible corruption of a game surface | Double `Release` |

**Tip**: `borderlessFullscreen 1` in `DPfix.ini` (borderless fullscreen, instant alt-tab). This is the
tested configuration.

## Installation

1. (Recommended) Copy `savedata\dp.sav` somewhere safe.
2. Download **`DPStabilityFix.exe`** from the [Releases page](../../releases). That one file is enough: the mod
   DLL, the settings files and the DPfix shaders are inside it. Close the game, then run it (the launcher).
3. **Installation** tab > "Install / update". If an original DPfix is installed, the "Disable the
   original DPfix" button renames it (nothing is deleted; `DPfix.ini` is kept) so the integrated, fixed
   version is used.
4. Adjust graphics and stability, then **Play**.

### The launcher

A single application (C#, Avalonia), copied into the game folder during installation. It finds the game by
itself (launcher folder, last folder chosen, Steam libraries). The language (English or French) is chosen
at the top right and remembered; it follows the Windows language at first launch.

| Tab | Content |
|---|---|
| **Graphics** | Display mode, resolution, anti-aliasing in plain words (from "Off" to "Excellent": ×9 supersampling + SMAA), ambient occlusion, shadows, reflections, depth of field, anisotropic filtering. Writes `DPfix.ini` while keeping its comments. |
| **Stability** | Options of `DPStabilityFix.ini` explained: protected saves, backups, controller, aiming fix, time precision, timer, frame limiter, diagnostics, integrated DPfix. |
| **Saves** | Dated backups of `dp.sav`, one-click restore (the current save is set aside first). |
| **Installation** | Status (game and mod version, 4 GB patch, DPfix), install / update, original DPfix, uninstall, Steam launch option, logs. |

The **Restore defaults** button puts DPfix.ini and DPStabilityFix.ini back to the shipped values
(anti-aliasing and effects off, mod options at their defaults), keeping your display mode and resolution.

**From Steam**: game Properties > General > Launch Options:
`"<game folder>\DPStabilityFix.exe" %command%` (the launcher shows the exact line, with a "Copy" button).
"Play" in Steam then opens the launcher; it hides during the game and closes with it, so that Steam
(overlay, playtime) sees the game running.

The installation:

- checks that it really is `DP.exe` (32-bit; warns if it is not the Steam 1.01b version) and that the game
  is not running;
- copies `DP.exe` to `DP.exe.dpsf-original` (once), then applies the **4 GB patch**: `DP.exe` is limited
  to 2 GB of memory, and the `LARGE_ADDRESS_AWARE` flag gives it ~4 GB;
- copies the mod DLL, the DPfix shaders (`dpfix\`), the launcher, and the `.ini` files
  (`DPStabilityFix.ini`, `DPfix.ini`, `DPfixKeys.ini`) if they do not exist yet: your settings are kept.

It can be run again at every update: the DLL and the shaders are replaced, the 4 GB patch is not applied
twice, and the original copy of `DP.exe` is never overwritten.

From the command line (no window): `DPStabilityFix.exe install "<DP.exe or game folder>"
[--disable-external-dpfix]` (exit code 0 on success).

A Steam file integrity check removes the 4 GB patch: run the installation again.

The mod creates a `DPStabilityFix\` folder next to `DP.exe`:

- `logs\`: one log per session;
- `crashdumps\`: one `.dmp` file per crash;
- `savebackups\`: backups of `dp.sav` (`dp_<date>_<reason>.sav`) and recovered interrupted writes
  (`recovered_*.sav`).

**Uninstalling**: "Uninstall" button in the launcher (DLL removed, original `DP.exe` restored; settings,
logs and backups kept). By hand: delete `X3DAudio1_7.dll`, then replace `DP.exe` with
`DP.exe.dpsf-original` (renamed to `DP.exe`).

### Restoring a save

**Saves** tab of the launcher. By hand: with the game closed, copy the wanted file from
`DPStabilityFix\savebackups\` to `savedata\dp.sav`.

## What to do when something goes wrong

After a crash, provide:

- the session log (`DPStabilityFix\logs\`, the most recent one);
- the matching `.dmp` file (`DPStabilityFix\crashdumps\`);
- what was happening in the game (chapter, place, action, cutscene...).

For a display problem, set `logLevel 2` in `DPfix.ini`: DPfix then details what it does in the same log
(set `logLevel 0` again afterwards).

To go back to the original DPfix: rename `d3d9.dll.dpfix-desactive` to `d3d9.dll` (the integrated
version then turns itself off).

## How it works

- **Loading**: `DP.exe` imports `X3DAudio1_7.dll` (DirectX 3D audio). Since Windows looks in the game
  folder first, our DLL is loaded in its place before the game code, and forwards the two audio functions
  to the real system DLL. Only `DP.exe` imports this DLL: no conflict with DPfix (`d3d9.dll`), PhysX or
  Steam.
- **Interception**: the mod redirects some entries of the import table of `DP.exe` (`CreateFileA`,
  `WriteFile`, `CloseHandle`, `Sleep`, `Direct3DCreate9`, `joyGetPosEx`...). Only calls made by the game
  are affected. `Present` and `Reset` are intercepted through the Direct3D device vtable. A few internal
  functions of `DP.exe` are patched with MinHook after their first bytes are checked.
- **Atomic saving**: when the game opens `dp.sav` for writing, it actually writes to `dp.sav.dpsf.tmp`
  (initialized with the current content). When the file is closed (or on every `FlushFileBuffers`), the
  copy replaces `dp.sav` in one atomic operation. A crash during the write leaves the old `dp.sav`
  untouched. (The game itself empties the file before rewriting it in one go: see
  [the DP.exe analysis](docs/analyse-dp-exe.md), in French.)
- **Integrated DPfix**: our interception of `Direct3DCreate9` wraps the system Direct3D object in DPfix's
  one, as its `d3d9.dll` used to do. The DPfix code is compiled into the same DLL (`third_party/dpfix`),
  with Detours (non-free) replaced by MinHook.
- **Controllers**: the game only reads controllers through `joyGetPosEx` and expects the Xbox 360 layout.
  The mod converts PlayStation controllers to that layout, refuses a malformed read the game also makes,
  and reads the controllers on a background thread.

## Building

Prerequisites:

- Build Tools for Visual Studio 2022 ("Desktop development with C++" workload), which provide MSVC, the
  Windows SDK, CMake and Ninja;
- .NET 10 SDK (`winget install Microsoft.DotNet.SDK.10`) for the launcher.

The first configuration downloads D3DX9 (Microsoft NuGet) and MinHook (GitHub), checked by SHA-256 hash,
as well as the launcher's NuGet packages: an Internet connection is required.

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1            # DLL + launcher -> build\x86-release\package
powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1        # tests against a fake DP.exe
dotnet test launcher                                                # launcher unit tests
```

In VS Code (C/C++, CMake Tools and C# Dev Kit extensions): open the folder, pick the `x86-release`
preset for the DLL; `launcher/DPStabilityFix.Launcher.slnx` for the launcher.

### Tests

`tests/fake_game` produces a fake `DP.exe` that imports the same functions as the game.
`tests/run-tests.ps1` checks with it: audio forwarding, atomic writes, a crash during a write then
recovery, closing with an open save, deletion, a crash with dump and chaining of the game's handler,
command line launcher (effective 4 GB patch, original copy, refusal while the game runs, DPfix files,
original DPfix), Direct3D 9 interception, frame rate measurements, "DP.exe style" time precision, the
controller conversion and the malformed read, and the integrated DPfix with the exact display parameters
of the game (windowed and borderless, rendering from another thread, render target displayed as a texture,
then `Reset`).

Each DPfix fix was verified once **without** the correction: the test then fails like the game does (same
error code).

`launcher/tests` (xUnit) covers the launcher logic: `.ini` files (comments kept), settings / DPfix.ini
mapping, the 4 GB patch, backups and restore, installation, default settings, and the translation table
(every text used exists in both languages with the same arguments).

New launcher texts go through `Core/Translations.cs` (French and English side by side), never into the
views or the code directly.

BEFORE / AFTER
<img width="1920" height="1080" alt="247660_20261004193418_1" src="https://github.com/user-attachments/assets/9ad66554-fe4e-489e-a8f2-479c1dd90fd3" />
<img width="1920" height="1080" alt="247660_20261004192909_1" src="https://github.com/user-attachments/assets/a97fe1b6-0491-4917-b082-037f54417ef2" />

<img width="1920" height="1080" alt="247660_20261004193421_1" src="https://github.com/user-attachments/assets/c92f487e-6220-4318-9089-f450b10ecbf6" />
<img width="1920" height="1080" alt="247660_20261004192914_1" src="https://github.com/user-attachments/assets/0d65b3d5-c161-42d2-9a22-78558395bb27" />


## License

Copyright (C) 2026 Cesar Schaal

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU
General Public License version 3** (or, at your option, any later version), as published by the Free
Software Foundation. It is distributed in the hope that it will be useful, but **without any warranty**.
See the [LICENSE](LICENSE) file.

### Third-party components

- **DPfix 0.9**, Copyright 2013 Peter Thoman (Durante), GPL-3.0-or-later: `third_party/dpfix/`,
  modifications marked in the files and listed in `third_party/dpfix/ORIGINE.md`.
- **SMAA**, Jimenez et al., MIT-style license (file headers).
- **VSSAO**, Tomerk (OBGE), adapted by Durante: license **to be verified** (not stated).
- **MinHook**, Tsuda Kageyu, BSD-2-Clause: downloaded at build time.
- **ZachFix**, h714je, GPL-3.0: restricted aiming fix (`src/patches/FpuPatches.cpp`) taken from its
  analysis and code (`gameplay/aim_fpu_fix.cpp`).
- **Avalonia** (MIT), **CommunityToolkit.Mvvm** (MIT): launcher, downloaded at build time.
- **D3DX9**, Microsoft (DirectX SDK license): headers and library downloaded at build time, not
  redistributed; at run time, the `d3dx9_43.dll` of the DirectX runtime installed with the game.
- Not included: DPfix's NVIDIA FXAA 3.11 shader ("ALL RIGHTS RESERVED", non-free).
