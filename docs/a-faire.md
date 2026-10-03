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

**Hypothèse (à vérifier)** : WinMM transmet les axes dans l'ordre déclaré par la manette.

| Axe WinMM | Xbox 360 (attendu par le jeu) | DualSense |
|---|---|---|
| X / Y | Stick gauche | Stick gauche |
| Z | Gâchettes LT/RT (centre au repos) | Stick droit horizontal |
| R | Stick droit vertical | Stick droit vertical |
| U | Stick droit horizontal | Gâchette L2 (0 au repos) |
| V | (inutilisé) | Gâchette R2 |

L2 au repos serait lu comme stick droit poussé à fond : caméra qui tourne sans fin. Boutons aussi dans
un autre ordre (Carré, Croix, Rond, Triangle au lieu de A, B, X, Y). `configJ.cnf` utilise des numéros
de boutons Xbox (INTERACT = 0 = A, etc.).

**Plan** :

1. Intercepter `joyGetPosEx` (et `joyGetDevCaps` pour identifier la manette : fabricant Sony `0x054C`,
   DualSense `0x0CE6`, DualSense Edge `0x0DF2`, DS4 `0x05C4` / `0x09CC`).
2. Journaliser le modèle, le nombre d'axes et de boutons, et les axes bruts (au repos, puis en
   mouvement) pour **vérifier la correspondance ci-dessus** sur la manette de l'utilisateur.
3. Pour une manette Sony, présenter au jeu une disposition Xbox 360 : U ← Z, Z ← centre + (L2 − R2)/2,
   boutons réordonnés (A ← Croix, B ← Rond, X ← Carré, Y ← Triangle, LB/RB ← L1/R1,
   Back/Start ← Create/Options, LS/RS ← L3/R3).
4. Option `.ini` pour désactiver le remappage, scénario de test avec une manette simulée.

En attendant : Steam Input (Propriétés du jeu > Manette) ou DS4Windows.

## Plantages et saccades

- **Plantage du chapitre 9** (épisode 2, sortie du diner) : pas encore traité. Il faut un fichier de
  diagnostic de ce plantage.
- **Saccade d'environ 94 ms toutes les 20 s** (observée le 03/10/2026, horodatée dans le journal) :
  cause inconnue.
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
