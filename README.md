# DPStabilityFix

Mod de stabilité pour **Deadly Premonition: The Director's Cut** (PC, version Steam 1.01b).

Objectifs, par priorité :

1. **Ne plus jamais perdre sa sauvegarde** à cause d'un plantage (le jeu n'utilise qu'un seul fichier,
   `savedata\dp.sav`).
2. **Comprendre puis corriger les plantages** (notamment épisode 2, chapitre 9).
3. **Réduire les saccades.**

Il complète [DPfix](https://github.com/PeterTh/dpfix) de Durante (résolution, anticrénelage, effets)
sans le remplacer ni le redistribuer. Les deux mods sont **conçus** pour s'installer ensemble (DLL
différentes, interception Direct3D posée sur l'objet que reçoit le jeu, DPfix compris), mais cette
cohabitation **n'a pas encore été testée en jeu**. Le journal indique quel `d3d9.dll` est utilisé.

## État actuel : version 0.1 (« étape 0 »)

| Fonction | État |
|---|---|
| Sauvegarde protégée : écriture atomique, copies de secours, récupération après plantage | **Actif** |
| Fichier de diagnostic (.dmp) et rapport détaillé à chaque plantage | **Actif** |
| Mesures de cadence d'images, des attentes (`Sleep`) et de la mémoire | **Actif** |
| Résolution du minuteur Windows à 1 ms (piste contre les saccades, à confirmer) | **Actif**, désactivable |
| Limiteur d'images précis | Disponible, désactivé par défaut |
| Patch 4 Go (`LARGE_ADDRESS_AWARE`) | Appliqué par l'installeur |
| Correctifs ciblés des plantages | À venir, d'après les diagnostics collectés |

Cette version **observe** surtout : les problèmes connus datent de 2013, et le comportement sur un
Windows actuel peut différer. Les correctifs viendront des journaux et diagnostics réels.

## Installation

1. (Recommandé) Copier `savedata\dp.sav` en lieu sûr.
2. Garder ensemble `DPStabilityFixSetup.exe`, `X3DAudio1_7.dll` et `DPStabilityFix.ini` (dossier
   `build\x86-release\package\` ou artefact de la CI), fermer le jeu, puis lancer
   `DPStabilityFixSetup.exe` et sélectionner `DP.exe` (ou glisser `DP.exe` sur l'installeur).
3. Lancer le jeu normalement depuis Steam.

L'installeur :

- vérifie qu'il s'agit bien de `DP.exe` (32 bits, version Steam 1.01b, sinon il demande confirmation)
  et que le jeu n'est pas lancé ;
- copie `DP.exe` en `DP.exe.dpsf-original` (une seule fois), puis applique le **patch 4 Go** :
  `DP.exe` est limité à 2 Go de mémoire, le drapeau `LARGE_ADDRESS_AWARE` lui donne ~4 Go ;
- copie la DLL du mod, et le `.ini` s'il n'existe pas déjà (vos réglages sont conservés).

En ligne de commande : `DPStabilityFixSetup.exe "<DP.exe ou dossier du jeu>" --quiet` (aucune
fenêtre, code de sortie 0 en cas de succès).

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
  l'écriture laisse l'ancien `dp.sav` intact.

## Compilation

Prérequis : Build Tools for Visual Studio 2022 (charge « Développement desktop en C++ »), qui
fournissent MSVC, le SDK Windows, CMake et Ninja.

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
tourne), interception Direct3D 9 et mesures de cadence.

## Licence

Copyright (C) 2026 Cesar Schaal

Ce programme est un logiciel libre : vous pouvez le redistribuer et/ou le modifier selon les termes
de la **GNU General Public License version 3** (ou, à votre choix, toute version ultérieure), telle
que publiée par la Free Software Foundation. Il est distribué dans l'espoir qu'il sera utile, mais
**sans aucune garantie**. Voir le fichier [LICENSE](LICENSE).

DPfix est un projet distinct de Durante (Peter Thoman), également sous GPL-3.0. Aucun code de DPfix
n'est inclus à ce jour ; tout emprunt futur sera signalé dans les fichiers concernés avec la mention
de son auteur.
