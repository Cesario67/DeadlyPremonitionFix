# À faire

Points en attente, avec ce que l'on sait déjà. À tenir à jour : retirer un point une fois traité.

## À tester en jeu

- **Sauvegarde** : faire une sauvegarde (téléphone ou fin de chapitre) et vérifier dans le journal
  « Écriture redirigée » puis « Sauvegarde validée ».
- **Alt-tab** en plein écran sans bordure et en vrai plein écran (DPfix intégré) : le jeu doit revenir.
- **SMAA, SSAO** de DPfix intégré : affichage correct (mini-carte, interface, lumières).
- Remettre `logLevel 0` dans le `DPfix.ini` de l'utilisateur une fois les tests DPfix terminés
  (passé à 2 le 03/10/2026 ; copie précédente : `DPfix.ini.avant-borderless`).

## Manette PlayStation (DualSense, DS4) : caméra qui tourne seule

Signalé le 03/10/2026 avec une manette PS5 : la caméra tourne autour du personnage, impossible de la
contrôler. Problème connu (forums Steam, déjà avec la DS4 ; contournement communautaire : DS4Windows
en émulation Xbox 360 avec la manette d'origine cachée).

**Vérifié dans le code** : DP.exe lit les manettes uniquement par `winmm!joyGetPosEx` (IAT `0x76E250`,
lecture principale en `0x709D95`, boucle sur les identifiants de manette, structures de `0x36` octets
avec le `JOYINFOEX` à `+2`). Ni XInput ni DirectInput. Les 6 axes (X, Y, Z, R, U, V) et les boutons
sont stockés bruts.

**Mesuré le 04/10/2026** (DualSense `0x0CE6` en Bluetooth, lue par `joyGetPosEx` comme le jeu, hors
jeu, Steam ouvert) ; la colonne Xbox 360 reste la disposition WinMM habituelle, non mesurée faute de
manette Xbox :

| Axe WinMM | Xbox 360 (attendu par le jeu) | DualSense (mesuré) |
|---|---|---|
| X / Y | Stick gauche | Stick gauche |
| Z | Gâchettes LT/RT (centre au repos) | Stick droit horizontal |
| R | Stick droit vertical | Stick droit vertical |
| U | Stick droit horizontal | Gâchette R2 (0 au repos) |
| V | (inutilisé) | Gâchette L2 (0 au repos) |

Au repos, R2 (U = 0) est lue par le jeu comme stick droit poussé à fond : caméra qui tourne sans fin.
Boutons DualSense mesurés : Carré 0, Croix 1, Rond 2, Triangle 3, L1 4, R1 5, L2 6, R2 7, Create 8,
Options 9, L3 10, R3 11, PS 12 (bits de `dwButtons`) ; clic du pavé tactile non transmis par WinMM ;
croix directionnelle sur `dwPOV` (0, 9000, 18000, 27000, comme une manette Xbox). `configJ.cnf` utilise des numéros de boutons Xbox (INTERACT = 0 = A,
etc.).

**Fait (non poussé)** : `src/input/` intercepte `joyGetPosEx`, identifie la manette par
`joyGetDevCaps` (fabricant Sony `0x054C`), et présente au jeu une disposition Xbox 360 : U ← Z,
Z ← centre + (L2 − R2)/2, V ← 0, boutons réordonnés (A ← Croix, B ← Rond, X ← Carré, Y ← Triangle,
LB/RB ← L1/R1, Back/Start ← Create/Options, LS/RS ← L3/R3). Options `[Controller]` dans
`DPStabilityFix.ini`, diagnostic des axes dans le journal.

**Reste à vérifier en jeu** : caméra au stick droit, sens des gâchettes (sinon `SwapTriggers=1`).

## Plantages et saccades

- **Plantage du chapitre 9** (épisode 2, sortie du diner) : pas encore traité. Il faut un fichier de
  diagnostic de ce plantage.
- **Saccade d'environ 65 à 94 ms toutes les 20 s** : cause mesurée le 04/10/2026. DP.exe appelle
  `joyGetPosEx` à chaque image pour les manettes 0 à 6, et toutes les 20 s l'appel sur un numéro vide
  (manette 1) bloque ~64 ms dans WinMM : 15 saccades sur 15 à 2 ms près d'un appel lent. Non reproduit
  hors du jeu (outil C# et faux jeu, avec ou sans boucle de messages, avec l'appel mal formé de DP.exe) :
  déclencheur propre au processus du jeu, peut-être l'overlay Steam. Correctif `[Controller]
  CacheAbsent=1` (réponse en cache, vérification en arrière-plan) **à vérifier en jeu** : plus de
  saccade, et aucun appel lent sur la manette 0 pendant les vérifications du thread d'arrière-plan.
- DP.exe fait aussi, à chaque image, un appel `joyGetPosEx(0)` mal formé (`dwSize` = 6, drapeaux
  `0x120C7064`) qui échoue toujours (code 165) : bug du jeu sans effet constaté.
- Effet réel de `ForceFpuPreserve` et de `TimerResolutionMs` sur les saccades : comparer deux sessions
  (option à 0 puis à 1), PC allumé depuis plusieurs jours.
- Sauvegarde tronquée au chargement : DP.exe complète un `dp.sav` trop court avec des zéros au lieu de
  le refuser (`0x408C80`). Idée : avertir et proposer la dernière copie saine si `dp.sav` est plus petit
  que la dernière sauvegarde validée.

## DPfix

- Écrire à Durante (Peter Thoman) : demande des sources de la 0.9.5 (non publiées) et de son accord
  pour l'intégration.
- Licence des shaders VSSAO (Tomerk, OBGE) à vérifier avant toute publication.

## Installeur

- Ajouter aux `.ini` déjà installés les nouvelles options d'une mise à jour (avec leur commentaire),
  sans toucher aux réglages existants.
