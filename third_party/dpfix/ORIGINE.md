# DPfix (Durante / Peter Thoman) : origine des fichiers

- Projet : https://github.com/PeterTh/dpfix
- Version importée : **0.9**, commit `050c562ba57f7e7b0d799cfcb7c993e1f609714f` (17/11/2013),
  dernière version dont les sources sont publiées. La 0.9.5 (03/12/2013, compatibilité DP.exe 1.01b)
  n'existe qu'en binaire.
- Licence : GPL-3.0-or-later (voir `LICENSE.txt` et les en-têtes des fichiers), Copyright 2013
  Peter Thoman (Durante).

## Import

Le premier commit de ce dossier contient les fichiers **tels quels**. Toute modification ultérieure
est faite dans des commits séparés et signalée en commentaire dans les fichiers concernés
(« Modifié pour DPStabilityFix : ... »), conformément à la GPL (section 5a).

## Modifications apportées (après l'import)

Chaque modification est signalée dans le fichier par un commentaire « Modifié pour DPStabilityFix ».

**Adaptation aux outils actuels (comportement inchangé)**

| Fichier | Modification |
|---|---|
| `d3d9.h` | Plus de `#pragma comment(lib)` (liaison par CMake, plus de `dxerr.lib`, pas d'import de `d3d9.dll`) ; plus de déclaration de l'export `Direct3DCreate9`. |
| `FXAA.h`, `GAUSS.h`, `SMAA.h`, `SSAO.h` | `<dxerr.h>` retiré (absent des SDK actuels). |
| `Detouring.h` | `<detours.h>` (Detours Express, non libre) remplacé par `DetoursShim.h` (MinHook). |
| `main.cpp`, `d3d9.cpp`, `d3d9.def` | Non compilés : remplacés par `src/graphics/dpfix_bridge/DpfixBridge.cpp`. |

**Corrections de bugs**

| Fichier | Bug | Conséquence avant correction |
|---|---|---|
| `RenderstateManager.cpp` (`releaseResources`) | `lastRTSurface`, `depthSurface`, `mainSurface` gardaient une référence pendant `Reset` | Tout `Reset` échouait (`D3DERR_INVALIDCALL`) dès que le jeu avait dessiné : jeu figé après un alt-tab en plein écran. Reproduit par le test `dpfix-reset`. |
| `RenderstateManager.cpp` (`redirectSetRenderTarget`) | `depthSurface = lastRTSurface` sans `AddRef`, puis deux `Release` | Surface du jeu relâchée une fois de trop (destruction prématurée possible). |
| `RenderstateManager.cpp` (`redirectSetRenderTarget`) | `bb1->Release()` sans vérifier `GetBackBuffer` | Plantage avec un seul tampon ou périphérique perdu. |
| `RenderstateManager.cpp` (`initResources`, `reloadAA`) | FXAA utilisé sans son shader | Plantage si `aaType FXAA` (shader non libre, non distribué) : SMAA utilisé dans tous les cas. |
| `Settings.cpp`, `KeyActions.cpp` | Boucle `while(!eof())` | Blocage infini au démarrage si `DPfix.ini` / `DPfixKeys.ini` est absent ou contient une ligne trop longue. |

Le `DPfix.ini` distribué (`dist/DPfix.ini`) est celui de la 0.9 avec `disableJoystick 0`, comme la
0.9.5 qui a retiré cette option.

## Fichiers volontairement non importés

| Fichier | Raison |
|---|---|
| `dinput.h` | En-tête du SDK DirectX de Microsoft, pas sous GPL. |
| `dinputWrapper.cpp/.h` | Chaînage d'un wrapper DirectInput, inutile ici. |
| `pack/dpfix/FXAA.h`, `pack/dpfix/FXAA.fx` | Shader NVIDIA FXAA 3.11 : « ALL RIGHTS RESERVED », aucune autorisation de redistribution. Non libre : l'anticrénelage passe par SMAA. |
| `DPfix.sln`, `DPfix.vcxproj`, `DPfix.v11.suo` | Projet Visual Studio 2012 remplacé par CMake. |
| `pack/dpfix/screens`, `tex_dump`, `tex_override` | Dossiers vides créés à l'exécution. |

## Licences des fichiers tiers inclus

| Fichier | Auteur | Licence |
|---|---|---|
| `src/AreaTex.h`, `src/SearchTex.h`, `shaders/SMAA.h`, `shaders/SMAA.fx` (en-tête) | Jimenez, Masia, Echevarria, Sousa, Gutierrez | Licence de type MIT (en-tête des fichiers) |
| `shaders/VSSAO.fx`, `shaders/VSSAO2.fx` | Tomerk (OBGE), adapté par Durante | **À vérifier** : aucune licence indiquée dans les fichiers |
| `shaders/GAUSS.fx` | Durante, d'après un article de rastergrid.com | GPL-3.0-or-later (projet DPfix) |
