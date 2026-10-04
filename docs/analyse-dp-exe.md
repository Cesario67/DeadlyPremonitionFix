# Analyse statique de DP.exe (Steam 1.01b)

Exécutable analysé : `DP.exe`, horodatage PE `0x529721DC`, base `0x400000`, sans ASLR ni SteamStub.
Désassemblage fait avec `dumpbin /disasm` (Build Tools) dans un dossier temporaire : **aucun code du
jeu n'est stocké dans ce dépôt**, seulement des adresses et des descriptions.

Légende : **vérifié** = lu dans le code (et/ou reproduit par un test) ; *hypothèse* = déduction non
encore confirmée en jeu.

## Imports utiles (adresses d'IAT)

| IAT | Fonction | Appels |
|---|---|---|
| `0x76E034` | `CreateFileA` | 10 |
| `0x76E038` | `WriteFile` | 9 |
| `0x76E028` | `Sleep` | 15 directs + 12 via registre |
| `0x76E080` / `0x76E078` | `QueryPerformanceCounter` / `Frequency` | 7 / 4 |
| `0x76E06C` | `GlobalMemoryStatus` | 1 |
| `0x76E14C` | `CreateThread` | 1 (fabrique de threads du moteur, `0x712A00`) |
| `0x76E180` | `SetUnhandledExceptionFilter` | 4 |
| `0x76E264` | `d3d9!Direct3DCreate9` | 1 (via thunk `0x73A9C4`, appelé en `0x6CC336`) |

## Sauvegarde (vérifié)

- Chemin : chaîne `"savedata/dp.sav"` (**barre oblique**) en `0x76F554` (écriture) et `0x76F5A0`
  (lecture). Les autres occurrences de la chaîne dans `.rdata`/`.data` ne sont pas référencées
  directement par le code.
- **Écriture** `0x408BD0` : `CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
  FILE_ATTRIBUTE_NORMAL, NULL)`, puis **un seul** `WriteFile` du tampon complet, puis `CloseHandle`.
  Pas de `FlushFileBuffers`, pas de fichier temporaire. Si le nombre d'octets écrits diffère, boîte
  de dialogue « WinSave / Save failed!!! ».
- **Lecture** `0x408C80` → `0x408B20` : `CreateFileA(..., GENERIC_READ, 0, NULL, OPEN_EXISTING, ...)`,
  `GetFileSize`, allocation, `ReadFile` du fichier entier. Si la taille lue diffère de la taille
  attendue, le jeu copie ce qu'il a et **complète avec des zéros** au lieu de refuser le fichier.
- Les deux sont appelées depuis un répartiteur de requêtes (`0x409790`) protégé par une section
  critique (`EnterCriticalSection`/`LeaveCriticalSection`), avec un code de commande (lecture,
  écriture...) : *hypothèse* : exécuté sur un thread de travail pendant que le jeu continue.

**Conséquences**

- `CREATE_ALWAYS` vide le fichier immédiatement : un plantage (de n'importe quel thread) avant la fin
  du `WriteFile` laisse un `dp.sav` vide ou tronqué. C'est exactement ce que corrige l'écriture
  atomique de DPStabilityFix (le jeu écrit dans `dp.sav.dpsf.tmp`, qui remplace `dp.sav` à la
  fermeture).
- *Hypothèse* : comme la lecture accepte un fichier tronqué (complété par des zéros), une sauvegarde
  à moitié écrite peut se charger dans un état incohérent, puis faire planter le jeu à la reprise.
  Cela correspondrait aux témoignages « Resume plante » après une corruption au chapitre 9.

## Mémoire et patch 4 Go (vérifié)

- `GlobalMemoryStatus` n'est appelé qu'en `0x4B0F40` : la fonction renvoie vrai si
  `dwAvailPhys >= 8 Mo`, en comparaison **non signée**. Le patch `LARGE_ADDRESS_AWARE` ne peut pas
  fausser ce test (piège classique des vieux jeux écarté).

## Temps et cadence

**Vérifié dans le code**

- `0x401F50` : renvoie un temps en **microsecondes depuis le démarrage du PC** :
  `QPC_absolu * (float)(1e6 / QPF)` (constante `1e6` en `0x76E658`), calcul x87 puis conversion
  entière (`_ftol2`). Appelée 7 fois.
- `0x701040` : renvoie `QPC_absolu / QPF` (secondes depuis le démarrage), calcul x87. Appelée 10 fois,
  notamment par des boucles à budget de temps (`0x40CFE0`, `0x40D0D0` : traitement de files tant que
  le temps écoulé reste sous un seuil).
- `0x6E4BD0` et `0x6EB5xx` calculent en revanche un **écart** (`QPC - départ`) avant la conversion :
  ils ne sont pas concernés par le problème ci-dessous.
- Création du périphérique en `0x6CC597` : `IDirect3D9::CreateDevice(0, HAL, hwnd, 0x44, ...)`, soit
  `D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED`, **sans `D3DCREATE_FPU_PRESERVE`**.

**Mécanisme (vérifié par test, sur la machine de développement)**

Sans `FPU_PRESERVE`, Direct3D 9 passe le x87 du thread qui crée le périphérique en **simple
précision** (24 bits de mantisse). Les calculs de `0x401F50` / `0x701040` portent sur la valeur
**absolue** du compteur, donc leur résolution dépend de la durée écoulée depuis le démarrage du PC :

| Allumé depuis | Résolution en simple précision |
|---|---|
| 1 h | ~0,25 ms |
| 1 jour | ~8 ms |
| 1 semaine | ~65 ms |

Le test `tests/run-tests.ps1` (scénario `frames` du faux jeu) reproduit le calcul de `0x401F50`
après un `CreateDevice` aux mêmes options, avec 7 jours simulés : pas de **65,5 ms** sans correctif,
**0,001 ms** avec `D3DCREATE_FPU_PRESERVE`.

Le **démarrage rapide** de Windows 8+ (activé par défaut) ne remet pas le compteur à zéro lors d'un
« Arrêter » : la durée s'accumule jusqu'au prochain « Redémarrer ». En 2013, sous Windows 7, le
compteur repartait de zéro à chaque démarrage.

*Hypothèse* : une partie des saccades sur PC récent vient de là (pas du temps grossier, budgets de
chargement mal mesurés). À confirmer en jeu : comparer les rapports de cadence avec
`PreciseGameTime=1` et `=0`, PC allumé depuis plusieurs jours.

**Le jeu a été conçu en simple précision** (établi par ZachFix, h714je, GPL-3.0) : la gestion de la
caméra de visée (`0x53B8B0`) borne un angle cible stocké en `float`, puis le compare par **égalité
stricte** à la borne restée dans un registre x87. En simple précision les deux valeurs sont égales ; en
double précision elles diffèrent et la caméra ne suit plus le réticule au bord de l'écran (« visée
restreinte », bug connu du portage PC). ZachFix l'a reproduit en forçant la double précision et
supprimé en revenant à la simple. Le jeu peut contenir d'autres comparaisons de ce type : la
double précision globale est donc à éviter.

**Correctif** (`src/patches/FpuPatches`, depuis le 04/10/2026) : le jeu reste en simple précision,
et la précision est réglée fonction par fonction, après vérification de leurs premiers octets :

- `0x401F50` et `0x701040` exécutées en **double précision** (`[Frames] PreciseGameTime`) ;
- `0x53B8B0` exécutée en **simple précision** (`[Gameplay] AimPrecisionGuard`), au cas où la double
  précision serait réactivée par ailleurs ;
- la précision de l'appelant est rétablie au retour.

Tests (faux jeu, 7 jours simulés) : x87 du jeu à 24 bits, pas du temps **0,001 ms** (65,5 ms sans le
correctif), visée en 24 bits même appelée en double précision.

Avant cette date, l'option `ForceFpuPreserve` ajoutait `D3DCREATE_FPU_PRESERVE` à `CreateDevice` :
double précision pour tout le jeu, donc visée restreinte probable. Elle reste disponible, désactivée
par défaut, pour comparaison.

## `Sleep`

- `0x408AF0` : `Sleep(f(arg))` générique.
- `0x6B3CB0` : boucle d'un thread de travail : `Sleep(intervalle_µs / 1000)` puis `Sleep(16)` à vide.
- `0x6CCF35`... : `Sleep(50)` en boucle d'attente (initialisation ou périphérique perdu, à confirmer).
- La boucle d'images principale n'est pas encore identifiée : les statistiques `Sleep` du thread de
  rendu dans le journal diront si elle dort, et combien.
