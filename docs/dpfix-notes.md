# Notes de lecture : DPfix

Source : [PeterTh/dpfix](https://github.com/PeterTh/dpfix), Durante (Peter Thoman), GPL-3.0, version 0.9
du 16/11/2013 (dernier commit le 17/11/2013). Lecture faite le 03/10/2026 pour connaître le jeu et
vérifier la cohabitation avec DPStabilityFix. **Aucun code n'a été copié.**

## Version 0.9.5 (binaire uniquement)

- Publiée le 03/12/2013 sur le blog de Durante : http://blog.metaclassofnil.com/?p=438. Le lien
  direct d'origine ne sert plus le fichier ; copie archivée :
  `http://web.archive.org/web/2014id_/http://blog.metaclassofnil.com/wp-content/uploads/2013/12/DPfix095.zip`
  (277 693 octets).
- Changements : compatibilité avec `DP.exe` 1.01b, **suppression de `disableJoystick`** (« désormais
  superflu »), AA appliqué dans plus de cas (menu en jeu), journal optionnel.
- **Sources non publiées** : le dépôt GitHub s'arrête à la 0.9 (2 commits, aucun tag, forks figés au
  17/11/2013) et le zip ne contient que `d3d9.dll`, les `.ini` et les shaders. Le README indique
  « I'll probably make the source code available at some point ». Toute intégration devra partir de
  la 0.9 ou obtenir les sources 0.9.5 auprès de Durante.
- Le README conseille de redémarrer le PC en cas de plantage, « le jeu lui-même y est assez
  sensible » : indice indépendant cohérent avec la perte de précision du temps du jeu selon la durée
  depuis le démarrage (voir `analyse-dp-exe.md`).
- Installée chez l'utilisateur le 03/10/2026 à côté de DPStabilityFix (premier test de cohabitation).

## Ce que fait DPfix 0.9 (sources) et ne fait pas

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

## Blocage après alt-tab en plein écran (observé le 03/10/2026)

- **Observé** (journal DPStabilityFix, DPfix 0.9.5, plein écran exclusif) : après un alt-tab, le jeu
  appelle `Reset` toutes les 50 ms ; les 696 appels échouent avec `D3DERR_INVALIDCALL` (`0x8876086C`)
  pendant 40 s, le jeu reste figé et doit être tué.
- **Cause probable** (vérifiée dans les sources 0.9, `RenderstateManager.cpp`) : DPfix obtient
  `depthSurface` / `mainSurface` par `GetRenderTarget` (référence ajoutée) pendant l'image et ne les
  relâche que dans son `Present` (`redirectPresent`). Son `Reset` appelle `releaseResources()`, qui ne
  les relâche pas. Si le périphérique est perdu en cours d'image, le jeu ne présente plus : les
  références restent, et Direct3D refuse tout `Reset` tant qu'une surface `D3DPOOL_DEFAULT` est
  référencée.
- **Contournements** :
  - `borderlessFullscreen 1` dans `DPfix.ini` (pas de perte du périphérique à l'alt-tab), recommandé
    par Durante depuis la 0.2 ;
  - DPStabilityFix `DPfixResetWorkaround=1` (pour un DPfix d'origine externe) : sur
    `D3DERR_INVALIDCALL`, appel du `Present` de DPfix, puis nouveau `Reset`. **Probablement
    inefficace** : la lecture complète du code a montré que DPfix garde aussi `lastRTSurface` en
    permanence, que son `Present` ne relâche pas.
  - **Correctif réel : DPfix intégré** (`third_party/dpfix`), dont `releaseResources` relâche toutes ces
    références. Vérifié par le test `dpfix-reset` du faux jeu : `Reset` renvoie `0x8876086C` sans le
    correctif (le code observé en jeu), `0x00000000` avec.

## Pistes pour la suite

- Si le journal montre des saccades, DPfix n'y est pour rien côté cadence : la piste `Sleep` /
  résolution du minuteur reste la nôtre.
- Le problème de visée/manette signalé par des joueurs avec DPfix 0.9 pouvait venir de
  `disableJoystick` (réponse vide de `joyGetPosEx`) ; l'option n'existe plus en 0.9.5.
