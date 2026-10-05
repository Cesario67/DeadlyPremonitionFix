## What's new in 0.2.1

- **Zero-delta guard**: avoids the freeze that happens when the frame delta is exactly zero (division by zero in the movement speed). Bug and fix found by [ZachFix](https://github.com/h714je/ZachFix); not triggered yet in my tests, so unproven in real conditions.
- **Loading diagnostics** in the log: duration of loading phases, long gaps between frames, near-zero frame intervals.
- **Checked in game** (15 min session, 1080p, DualSense): 60 FPS steady, atomic saves with backups and reload, ~1 s new-game loading.
- Aiming fix now documented as an experimental workaround (root cause not proven). Linux (Proton) is on the roadmap but untested.
- VSSAO shader: no license is stated upstream. It is included as DPfix distributes it and will be removed on request of any author.

## Nouveautés de la 0.2.1

- **Garde du delta nul** : évite le gel quand l'intervalle entre deux images est exactement nul (division par zéro dans la vitesse de déplacement). Bug et correctif trouvés par [ZachFix](https://github.com/h714je/ZachFix) ; pas encore déclenché dans mes tests, donc non prouvé en conditions réelles.
- **Diagnostic des chargements** dans le journal : durée des phases de chargement, longues interruptions, intervalles d'images quasi nuls.
- **Vérifié en jeu** (session de 15 min, 1080p, DualSense) : 60 i/s stables, sauvegardes atomiques avec copies de secours et rechargement, chargement de « Nouvelle partie » en environ 1 s.
- Correctif de visée désormais présenté comme un contournement expérimental (cause racine non prouvée). Linux (Proton) est dans la feuille de route mais non testé.
- Shader VSSAO : aucune licence indiquée en amont. Il est inclus tel que DPfix le distribue et sera retiré à la demande d'un auteur.

---

## Download

**Download `DPStabilityFix.exe` from the Assets below.** That single file is all you need (ignore "Source code": it is only for developers).

1. Close the game, then run `DPStabilityFix.exe`.
2. **Installation** tab > **Install / update**.
3. Adjust the graphics and stability options, then click **Play**.

The launcher is available in English and French (selector at the top right). Your saves and settings are never overwritten: the original `DP.exe` is kept as `DP.exe.dpsf-original`, and the **Uninstall** button restores it.

This is a pre-release: some fixes are verified only by tests, not yet in game. See the [README](https://github.com/Cesario67/DeadlyPremonitionFix#readme) for the list of what is verified. Bug reports and crash dumps are welcome in the issues.

## Téléchargement

**Téléchargez `DPStabilityFix.exe` dans les Assets ci-dessous.** Ce seul fichier suffit (ignorez « Source code » : c'est réservé aux développeurs).

1. Fermez le jeu, puis lancez `DPStabilityFix.exe`.
2. Onglet **Installation** > **Installer / mettre à jour**.
3. Réglez les graphismes et la stabilité, puis cliquez sur **Jouer**.

Le launcher existe en français et en anglais (sélecteur en haut à droite). Vos sauvegardes et réglages ne sont jamais écrasés : le `DP.exe` d'origine est conservé sous `DP.exe.dpsf-original`, et le bouton **Désinstaller** le restaure.

Version préliminaire : certains correctifs sont vérifiés par des tests, pas encore en jeu. Voir le [README](https://github.com/Cesario67/DeadlyPremonitionFix/blob/main/README.fr.md).

---

