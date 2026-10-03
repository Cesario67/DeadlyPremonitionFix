# DPStabilityFix

Mod pour **Deadly Premonition: The Director's Cut** (PC, version Steam 1.01b), qui vise à corriger
tous les problèmes connus du portage PC.

Objectifs, par priorité :

1. **Ne plus jamais perdre sa sauvegarde** à cause d'un plantage (le jeu n'utilise qu'un seul fichier,
   `savedata\dp.sav`).
2. **Comprendre puis corriger les plantages** (notamment épisode 2, chapitre 9).
3. **Réduire les saccades.**
4. **Graphismes** : il intègre [DPfix](https://github.com/PeterTh/dpfix) de Durante (résolution,
   anticrénelage SMAA, SSAO, profondeur de champ, plein écran sans bordure...), à partir de ses
   sources 0.9, avec de nombreux bugs corrigés (voir plus bas).

## État actuel : version 0.2

| Fonction | État | Vérifié en jeu |
|---|---|---|
| Sauvegarde protégée : écriture atomique, copies de secours, récupération après plantage | **Actif** | Pas encore (aucune sauvegarde faite) ; vérifié par les tests |
| Fichier de diagnostic (.dmp) et rapport détaillé à chaque plantage | **Actif** | Oui |
| Mesures de cadence d'images, des attentes (`Sleep`) et de la mémoire | **Actif** | Oui |
| Résolution du minuteur Windows à 1 ms | **Actif**, désactivable | Appliqué ; effet sur les saccades à confirmer |
| Calculs de temps du jeu en pleine précision (`D3DCREATE_FPU_PRESERVE`, voir [l'analyse](docs/analyse-dp-exe.md)) | **Actif**, désactivable | Appliqué (x87 53 bits) ; effet sur les saccades à confirmer |
| Limiteur d'images précis | Disponible, désactivé par défaut | Non |
| Patch 4 Go (`LARGE_ADDRESS_AWARE`) | Appliqué par l'installeur | Oui (4 Go d'espace d'adressage) |
| DPfix intégré et corrigé | **Actif** (réglages dans `DPfix.ini`), inactif si un DPfix d'origine (`d3d9.dll`) est présent | Lancement 1080p sans bordure : oui. Alt-tab, SMAA, SSAO : pas encore |
| Correctifs ciblés des plantages du jeu | À venir, d'après les diagnostics collectés | |

### DPfix : bugs corrigés

Le code publié de DPfix est la version 0.9, antérieure à la version 1.01b du jeu. Les corrections
suivantes ont été trouvées en jeu ou en lisant le code, et chacune est reproduite par un test (détails
dans [third_party/dpfix/ORIGINE.md](third_party/dpfix/ORIGINE.md)) :

| Symptôme | Cause |
|---|---|
| Jeu figé dès son premier changement de mode, ou après un alt-tab en plein écran | Références jamais relâchées (`QueryInterface` à chaque texture affichée, surfaces gardées pendant `Reset`) ; c'était aussi une **fuite de mémoire continue** |
| Plantage au lancement en mode sans bordure ou fenêtré | Fréquence de 59 Hz demandée par le jeu conservée en mode fenêtré |
| Mode sans bordure réappliqué à chaque image, raccourcis clavier inactifs | Fenêtre cherchée avec `GetActiveWindow()` depuis le thread de rendu |
| Jeu bloqué au démarrage si `DPfix.ini` manque | Boucle de lecture infinie |
| Plantage avec FXAA | Shader non libre absent : SMAA utilisé à la place |
| Corruption possible d'une surface du jeu | Double `Release` |

**Conseil** : `borderlessFullscreen 1` dans `DPfix.ini` (plein écran sans bordure, alt-tab
instantané). C'est la configuration testée.

## Installation

1. (Recommandé) Copier `savedata\dp.sav` en lieu sûr.
2. Garder ensemble tout le contenu du paquet (`DPStabilityFixSetup.exe`, `X3DAudio1_7.dll`, les
   `.ini`, le dossier `dpfix\` : dossier `build\x86-release\package\` ou artefact de la CI), fermer le
   jeu, puis lancer `DPStabilityFixSetup.exe` et sélectionner `DP.exe` (ou glisser `DP.exe` sur
   l'installeur).
3. Si un DPfix d'origine est installé, l'installeur propose de le désactiver (`d3d9.dll` renommé, pas
   supprimé ; `DPfix.ini` conservé) pour utiliser la version intégrée et corrigée.
4. Lancer le jeu normalement depuis Steam.

L'installeur :

- vérifie qu'il s'agit bien de `DP.exe` (32 bits, version Steam 1.01b, sinon il demande confirmation)
  et que le jeu n'est pas lancé ;
- copie `DP.exe` en `DP.exe.dpsf-original` (une seule fois), puis applique le **patch 4 Go** :
  `DP.exe` est limité à 2 Go de mémoire, le drapeau `LARGE_ADDRESS_AWARE` lui donne ~4 Go ;
- copie la DLL du mod et les shaders de DPfix (`dpfix\`), et les `.ini` (`DPStabilityFix.ini`,
  `DPfix.ini`, `DPfixKeys.ini`) s'ils n'existent pas déjà : vos réglages sont conservés.

L'installeur peut être relancé à chaque mise à jour : la DLL et les shaders sont remplacés, le patch 4
Go n'est pas réappliqué, la copie d'origine de `DP.exe` n'est jamais écrasée. Les nouvelles options
d'une mise à jour n'apparaissent pas dans des `.ini` déjà installés, mais s'appliquent avec leur
valeur par défaut.

En ligne de commande : `DPStabilityFixSetup.exe "<DP.exe ou dossier du jeu>" --quiet
[--disable-external-dpfix]` (aucune fenêtre, code de sortie 0 en cas de succès).

Une vérification de l'intégrité des fichiers par Steam retire le patch 4 Go : relancer l'installeur.

Le mod crée un dossier `DPStabilityFix\` à côté de `DP.exe` :

- `logs\` : un journal par session ;
- `crashdumps\` : un fichier `.dmp` par plantage ;
- `savebackups\` : copies de secours de `dp.sav` (`dp_<date>_<raison>.sav`) et écritures interrompues
  récupérées (`recovered_*.sav`).

**Désinstallation** : supprimer `X3DAudio1_7.dll`, puis remplacer `DP.exe` par `DP.exe.dpsf-original`
(renommé en `DP.exe`). Le dossier `DPStabilityFix\` peut être gardé (journaux, copies de secours).

### Restaurer une sauvegarde

Jeu fermé, copier le fichier voulu de `DPStabilityFix\savebackups\` vers `savedata\dp.sav`.

## Que faire en cas de problème

Après un plantage, fournir :

- le journal de la session (`DPStabilityFix\logs\`, le plus récent) ;
- le fichier `.dmp` correspondant (`DPStabilityFix\crashdumps\`) ;
- ce qui se passait dans le jeu (chapitre, lieu, action, cinématique...).

Pour un problème d'affichage, mettre `logLevel 2` dans `DPfix.ini` : DPfix détaille alors ce qu'il
fait dans le même journal (remettre `logLevel 0` ensuite).

Pour revenir au DPfix d'origine : renommer `d3d9.dll.dpfix-desactive` en `d3d9.dll` (la version
intégrée se désactive alors toute seule).

## Fonctionnement

- **Chargement** : `DP.exe` importe `X3DAudio1_7.dll` (audio 3D DirectX). Windows cherchant d'abord
  dans le dossier du jeu, notre DLL est chargée à sa place avant le code du jeu, et transmet les deux
  fonctions audio à la vraie DLL du système. Seul `DP.exe` importe cette DLL : pas de conflit avec
  DPfix (`d3d9.dll`), PhysX ou Steam.
- **Interception** : le mod redirige certaines entrées de la table d'imports de `DP.exe`
  (`CreateFileA`, `WriteFile`, `CloseHandle`, `Sleep`, `Direct3DCreate9`...). Seuls les appels du jeu
  sont concernés. `Present` et `Reset` sont interceptés via la vtable du périphérique Direct3D.
- **Sauvegarde atomique** : quand le jeu ouvre `dp.sav` en écriture, il écrit en réalité dans
  `dp.sav.dpsf.tmp` (initialisé avec le contenu actuel). À la fermeture du fichier (ou à chaque
  `FlushFileBuffers`), la copie remplace `dp.sav` en une opération atomique. Un plantage pendant
  l'écriture laisse l'ancien `dp.sav` intact. (Le jeu, lui, vide le fichier avant de le réécrire en
  une fois : voir [l'analyse de DP.exe](docs/analyse-dp-exe.md).)
- **DPfix intégré** : notre interception de `Direct3DCreate9` enveloppe l'objet Direct3D du système
  dans celui de DPfix, comme le faisait son `d3d9.dll`. Le code de DPfix est compilé dans la même DLL
  (`third_party/dpfix`), avec Detours (non libre) remplacé par MinHook.

## Compilation

Prérequis : Build Tools for Visual Studio 2022 (charge « Développement desktop en C++ »), qui
fournissent MSVC, le SDK Windows, CMake et Ninja. La première configuration télécharge D3DX9 (NuGet
Microsoft) et MinHook (GitHub), vérifiés par empreinte SHA-256 : une connexion Internet est nécessaire.

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1            # x86-release
powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1        # tests sur un faux DP.exe
```

Dans VS Code (extensions C/C++ et CMake Tools) : ouvrir le dossier, choisir le preset `x86-release`.

### Tests

`tests/fake_game` produit un faux `DP.exe` qui importe les mêmes fonctions que le jeu.
`tests/run-tests.ps1` y vérifie : transmission audio, écritures atomiques, plantage pendant une
écriture puis récupération, fermeture avec sauvegarde ouverte, suppression, plantage avec dump et
chaînage du gestionnaire du jeu, installeur (patch 4 Go effectif, copie d'origine, refus si le jeu
tourne, fichiers de DPfix, DPfix d'origine), interception Direct3D 9, mesures de cadence, précision du
temps « façon DP.exe », et DPfix intégré avec les paramètres d'affichage exacts du jeu (fenêtré et
sans bordure, rendu depuis un autre thread, cible de rendu affichée comme texture, puis `Reset`).

Chaque correctif de DPfix a été vérifié une fois **sans** la correction : le test échoue alors comme
le jeu (même code d'erreur).

## Licence

Copyright (C) 2026 Cesar Schaal

Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes
de la **GNU General Public License version 3** (ou, à votre choix, toute version ultérieure), telle
que publiée par la Free Software Foundation. Il est distribué dans l'espoir qu'il sera utile, mais
**sans aucune garantie**. Voir le fichier [LICENSE](LICENSE).

### Composants tiers

- **DPfix 0.9**, Copyright 2013 Peter Thoman (Durante), GPL-3.0-or-later : `third_party/dpfix/`,
  modifications signalées dans les fichiers et listées dans `third_party/dpfix/ORIGINE.md`.
- **SMAA**, Jimenez et al., licence de type MIT (en-têtes des fichiers).
- **VSSAO**, Tomerk (OBGE), adapté par Durante : licence **à vérifier** (non indiquée).
- **MinHook**, Tsuda Kageyu, BSD-2-Clause : téléchargé à la compilation.
- **D3DX9**, Microsoft (licence du SDK DirectX) : en-têtes et bibliothèque téléchargés à la
  compilation, non redistribués ; à l'exécution, `d3dx9_43.dll` du runtime DirectX installé avec le jeu.
- Non inclus : le shader NVIDIA FXAA 3.11 de DPfix (« ALL RIGHTS RESERVED », non libre).
