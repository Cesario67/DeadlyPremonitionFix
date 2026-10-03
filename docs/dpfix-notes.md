# Notes de lecture : DPfix

Source : [PeterTh/dpfix](https://github.com/PeterTh/dpfix), Durante (Peter Thoman), GPL-3.0, version 0.9
du 16/11/2013 (dernier commit le 17/11/2013). Lecture faite le 03/10/2026 pour connaître le jeu et
vérifier la cohabitation avec DPStabilityFix. **Aucun code n'a été copié.**

## Ce que fait DPfix (et ne fait pas)

- Wrapper `d3d9.dll` complet : classes `hkIDirect3D9` / `hkIDirect3DDevice9` qui héritent des
  interfaces Direct3D 9 et délèguent à la vraie DLL de `System32` (chargée dans son `DllMain`).
- Rendu : résolution interne, FXAA/SMAA, SSAO, profondeur de champ, ombres, reflets, HUD, captures,
  remplacement de textures.
- Hooks *inline* globaux (Microsoft Detours, donc pour tous les modules du processus) sur :
  - `D3DXCreateTextureFromFileInMemory(Ex)` de `d3dx9_43.dll` (textures) ;
  - `AdjustWindowRect(Ex)` et `SetWindowPos` de `user32.dll` (modes fenêtré/sans bordure) ;
  - `joyGetPosEx` de `winmm.dll` quand `disableJoystick` est actif, ce qui est **le cas par défaut**
    dans le `DPfix.ini` fourni (« works around game input bug »).
- **Aucune adresse codée en dur dans DP.exe** : DPfix ne dépend pas de la version précise de
  l'exécutable. Il date d'avant `DP.exe` 1.01b (28/11/2013) mais devrait donc s'y appliquer.
- **Rien sur la cadence, les sauvegardes ou la mémoire.** Des hooks de `SleepEx`, `timeGetTime` et
  `QueryPerformanceCounter` existent dans `Detouring.cpp` mais sont commentés (expérience
  abandonnée pour accélérer l'intro). L'option `enableVsync` est déclarée mais jamais utilisée.

## Correctifs de plantage déjà présents dans DPfix

D'après `pack/VERSIONS.txt` :

- 0.2 : le mode plein écran sans bordure « corrige aussi le plantage à l'alt-tab ».
- 0.6 : plantage causé par son propre remplacement de textures.
- 0.7 : « plantage à l'accès au menu Add-ons (corrige peut-être d'autres plantages) ».

Ces cas sont donc à vérifier **avec DPfix installé** avant de les attribuer au jeu.

## Cohabitation avec DPStabilityFix

- Pas de conflit de fichier : DPfix est `d3d9.dll`, nous sommes `X3DAudio1_7.dll`.
- Pas de conflit de hook : DPfix détourne `d3dx9_43`, `user32` et `winmm` ; nous ne touchons qu'à
  l'IAT de `DP.exe` pour `kernel32` (fichiers, `Sleep`, exceptions) et `d3d9!Direct3DCreate9`.
- Chaîne Direct3D attendue : le jeu appelle notre `Direct3DCreate9` → celui de DPfix renvoie un
  `hkIDirect3D9` → nous patchons sa vtable (`CreateDevice`) → DPfix renvoie un `hkIDirect3DDevice9`
  (le constructeur remplace `*ppReturnedDeviceInterface`) → nous patchons sa vtable (`Present`,
  `Reset`). Nos mesures passent donc **avant** le travail de DPfix dans `Present` (HUD, captures,
  fenêtre), ce qui est voulu : on mesure ce que voit le jeu.
- DPfix modifie les `D3DPRESENT_PARAMETERS` dans son `CreateDevice` (fenêtré/plein écran,
  résolution, fréquence). Notre journal les note **avant** l'appel : ce sont les valeurs demandées
  par le jeu, pas celles appliquées par DPfix.
- **Non vérifié en jeu à ce jour.**

## Pistes pour la suite

- Si le journal montre des saccades, DPfix n'y est pour rien côté cadence : la piste `Sleep` /
  résolution du minuteur reste la nôtre.
- Le problème de visée/manette signalé par des joueurs avec DPfix pourrait venir de
  `disableJoystick` (réponse vide de `joyGetPosEx`) : hors de notre périmètre, à garder en tête.
