# Projet

**DPStabilityFix** : mod pour *Deadly Premonition: The Director's Cut* (version PC Steam 2013,
compatible GOG si le code est identique) qui vise à **corriger tous les problèmes connus**. Objectifs,
par priorité :

1. Empêcher la corruption de la sauvegarde (`savedata\dp.sav`, fichier unique) en cas de plantage.
2. Diagnostiquer puis corriger les plantages connus (notamment épisode 2, chapitre 9, en sortie du diner).
3. Régulariser la cadence d'images (saccades, verrou 30 FPS mal tenu).
4. Graphismes : **DPfix (Durante) est intégré** dans la DLL (`third_party/dpfix`, sources 0.9) et corrigé.
   Il se désactive tout seul si un DPfix d'origine (`d3d9.dll`) est présent dans le dossier du jeu.

- Jeu installé dans : `E:\SteamLibrary\steamapps\common\Deadly Premonition The Director's Cut`
- Exécutable : `DP.exe` (32 bits, sans SteamStub, voir ci-dessous).

## Architecture

Analyse détaillée du code de `DP.exe` (sauvegarde, temps, mémoire, adresses) : `docs/analyse-dp-exe.md`.
Pour toute nouvelle analyse, désassembler dans le scratchpad (`dumpbin /disasm:nobytes`), jamais dans
le dépôt, et consigner les résultats (adresses + description, en distinguant vérifié / hypothèse) dans
ce document.

Faits vérifiés sur `DP.exe` 1.01b (horodatage PE `0x529721DC`) : 32 bits, **pas de SteamStub**
(sections standard, code lisible), pas d'ASLR (base `0x400000`), pas de `LARGE_ADDRESS_AWARE`, CRT
statique, fichiers via `CreateFileA`/`WriteFile`/`CloseHandle`, cadence via `Sleep` +
`QueryPerformanceCounter`, son propre `SetUnhandledExceptionFilter`. `PhysXLoader.dll` vient de PhysX
Legacy (`Program Files (x86)\NVIDIA Corporation\PhysX\Common`, via le PATH).

- DLL proxy **`X3DAudio1_7.dll`** (2 exports, importée uniquement par `DP.exe`) : stubs `naked` qui
  sautent vers la vraie DLL de `SysWOW64`, résolue au premier appel.
- Interception par **IAT de DP.exe** (`core/Hooking`) et vtables COM pour Direct3D 9. **MinHook**
  (téléchargé par CMake) ne sert qu'à DPfix, via `graphics/dpfix_bridge/DetoursShim` (API Detours
  réimplémentée) ; préférer l'IAT pour notre propre code.
- **DPfix intégré** : notre `Direct3DCreate9` enveloppe l'objet du système dans `hkIDirect3D9` de DPfix
  (`graphics/dpfix_bridge/DpfixBridge`), qui remplace son `main.cpp`/`d3d9.cpp`. DPfix lit `DPfix.ini`,
  `DPfixKeys.ini` et ses shaders (`dpfix\`) à côté de `DP.exe`.
- Modules, une responsabilité chacun :
  - `core/` : chemins, config `.ini`, journal, hooking, infos système.
  - `proxy/` : transmission de X3DAudio.
  - `save/` : écriture atomique de `dp.sav` (fichier temporaire + `MoveFileExW`), copies de secours.
  - `crash/` : filtre d'exceptions non gérées chaîné avec celui du jeu, minidumps, rapport.
  - `framepacing/` : mesures de cadence, statistiques de `Sleep`, résolution du minuteur, limiteur.
  - `graphics/` : `Direct3DCreate9` → `CreateDevice` → `Present`/`Reset`.
  - `patches/` (à venir) : correctifs binaires localisés par **signature**, jamais par adresse fixe.
- `DllMain` ne fait que des opérations sûres sous le verrou du chargeur ; ce qui charge des DLL est
  différé au premier `Direct3DCreate9` (`LateInit`).
- Chaque fonctionnalité doit pouvoir être désactivée dans le `.ini` (`dist/DPStabilityFix.ini`).
- Toute nouvelle fonctionnalité doit être couverte par un scénario du faux jeu (`tests/fake_game`) et
  une vérification dans `tests/run-tests.ps1`.

## Commandes

- Compiler : `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (preset `x86-release`).
- Tester : `powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1`.
- Installeur `setup/` → `build\<preset>\package\DPStabilityFixSetup.exe` (+ DLL + `.ini`) :
  l'utilisateur sélectionne `DP.exe`, l'installeur copie l'original (`DP.exe.dpsf-original`), applique
  le patch 4 Go et copie le mod. Il **modifie DP.exe** : ne jamais le lancer sur le vrai jeu sans
  confirmation de l'utilisateur. Mode test : `DPStabilityFixSetup.exe "<chemin>" --quiet`.
- Les scripts `.ps1` doivent rester en UTF-8 **avec BOM** (sinon PowerShell 5.1 abîme les accents).

## Toolchain

- **Build Tools for Visual Studio 2022** (pas l'IDE Visual Studio) : MSVC 14.44, SDK Windows
  10.0.26100, CMake et Ninja fournis par les Build Tools. Éditeur : VS Code (extensions C/C++ et
  CMake Tools).
- Cible **Win32 (x86)** obligatoire (le jeu est 32 bits) : compiler depuis un environnement
  `vcvarsall.bat x86` (ou `amd64_x86`).
- C++20, `/W4`, avertissements traités comme des erreurs.
- Code tiers dans `third_party/` avec sa licence : `third_party/dpfix` (GPL-3.0+). Il est compilé à
  part (bibliothèque `dpfix`, `/W3`, mode permissif, C++20) : ne pas l'aligner sur nos conventions.
  Toute modification y est **minimale**, marquée « Modifié pour DPStabilityFix » en commentaire, et
  listée dans `third_party/dpfix/ORIGINE.md`.
- Dépendances non libres ou volumineuses (**D3DX9** de Microsoft, **MinHook**) : téléchargées par
  `FetchContent` avec empreinte SHA-256, jamais stockées dans le dépôt.
- `d3dx9_43.dll` est en chargement différé (`/DELAYLOAD`) : la DLL doit rester chargeable sans le
  runtime DirectX (CI, jeu sans DPfix).
- Toute copie d'un binaire compilé (paquet, faux jeu) se fait en `POST_BUILD` **de la cible qui le
  produit** : attachée à une autre cible, la copie n'est pas refaite et les tests tournent sur un binaire
  périmé (déjà arrivé).
- Pour prouver qu'un test détecte un bug, le vérifier une fois **sans** le correctif (il doit échouer).
- CI GitHub Actions (`.github/workflows/build.yml`) : compilation + tests sans GPU ni DirectX
  (`-SkipSystemDependent`), DLL publiée en artefact.

## Conventions C++

- Typer explicitement les signatures de fonctions et les membres ; `auto` seulement quand le type est
  évident à la lecture (itérateurs, `make_unique`, casts).
- Pas de `new`/`delete` nus : RAII partout (`std::unique_ptr`, wrappers de `HANDLE` avec deleter).
- Respecte le principe de responsabilité unique : un fichier/une classe = une responsabilité claire.
  Pas de logique métier dans les fonctions de hook : le hook délègue à un module testable.
- Les hooks doivent être **défensifs** : ne jamais lancer d'exception à travers une frontière de hook,
  toujours appeler la fonction d'origine si notre traitement échoue, journaliser l'erreur.
- Pas d'allocation ni d'E/S bloquante dans les chemins chauds (hook de `Present`, boucle de jeu).
- Nommage : `PascalCase` pour les types, `camelCase` pour les fonctions et variables, `kConstante`
  pour les constantes, `g_` pour les globales (à limiter au strict nécessaire).
- Chaque adresse, offset ou signature issu de la rétro-ingénierie est documenté en commentaire :
  ce que fait le code ciblé, comment il a été identifié, version de `DP.exe` concernée.

## Licence

- Projet sous **GPL-3.0-or-later** (`LICENSE`, copyright Cesar Schaal). Le paquet distribué doit
  contenir `LICENSE.txt`.
- Le code de DPfix ([PeterTh/dpfix](https://github.com/PeterTh/dpfix), GPL-3.0-or-later) est intégré
  dans `third_party/dpfix` (import tel quel puis modifications signalées, voir ORIGINE.md).
- Ne jamais redistribuer le **binaire d'origine** de DPfix (`d3d9.dll` de Durante, demande explicite de
  son auteur) : notre version est compilée depuis les sources.
- Ne jamais intégrer de code ou de shader sans licence libre vérifiée (ex. : FXAA 3.11 de NVIDIA exclu ;
  VSSAO de Tomerk/OBGE à vérifier avant publication).
- Ce que l'on sait de DPfix (hooks, cohabitation, plantages qu'il corrige déjà) : `docs/dpfix-notes.md`.

## Sécurité des données du joueur et du jeu

- **Ne jamais modifier les fichiers du jeu** (`DP.exe`, DLL, données) ni `savedata\` sans
  confirmation explicite de l'utilisateur. La lecture et l'analyse sont autorisées.
- Toute opération sur `dp.sav` doit d'abord en garder une copie.
- Le déploiement de la DLL dans le dossier du jeu se fait sur demande, jamais automatiquement.
- Ne jamais committer de fichiers du jeu (exécutable, données, sauvegardes, minidumps contenant de la
  mémoire du jeu) : seulement notre code et des signatures.

## Workflow avec Claude Code

- L'utilisateur a de solides bases en **C#**, pas en C++ : expliquer les concepts C++ et Win32 nouveaux
  avec des équivalents C# quand c'est pertinent (pointeurs de fonction / délégués, RAII / `IDisposable`,
  `HANDLE` / `SafeHandle`, etc.).
- Ne jamais lancer de build (`msbuild`, `cmake --build`...) ni le jeu de ta propre initiative :
  toujours demander confirmation avant.
- Claude ne peut pas jouer : les tests en jeu sont faits par l'utilisateur, qui remonte journaux et
  minidumps. Préciser à chaque itération exactement quoi tester et quoi rapporter.
- Les commandes `git` (y compris commit/push) sont autorisées.
- Toujours travailler sur la branche **`dev`** (commits et push sur `dev`, jamais directement sur `main`).
- Ne pas utiliser de tiret cadratin (« — ») dans les livrables écrits : commentaires de code,
  documentation, messages de commit. Préférer des virgules, deux-points, parenthèses ou des phrases
  séparées. (Sans importance dans les réponses de chat.)
- Ne pas présenter une hypothèse sur le fonctionnement interne du jeu comme un fait : distinguer ce qui
  est vérifié (désassemblage, dump, test) de ce qui est supposé.

## Style de réponse

Adaptatif :
- Sujet simple : direct et concis.
- Sujet complexe, ou contexte que l'utilisateur semble ne pas maîtriser (C++, Win32, rétro-ingénierie) :
  expliquer en détail le raisonnement, et proposer les alternatives avec leurs compromis.
- Quand tu proposes une implémentation, justifie brièvement les choix d'architecture.
