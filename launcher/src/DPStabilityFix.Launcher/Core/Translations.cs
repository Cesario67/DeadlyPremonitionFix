namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// Table des textes du launcher : clé, puis (français, anglais). Les arguments de mise en forme sont
/// <c>{0}</c>, <c>{1}</c>... dans les deux langues. Un test vérifie que chaque clé employée dans les vues et
/// le code existe, a les deux langues et les mêmes arguments.
/// </summary>
public static class Translations
{
    public static readonly IReadOnlyDictionary<string, (string Fr, string En)> Table =
        new Dictionary<string, (string Fr, string En)>
        {
            // --- Fenêtre principale
            ["WindowTitle"] = ("DPStabilityFix : Deadly Premonition", "DPStabilityFix: Deadly Premonition"),
            ["GameFolder"] = ("Dossier du jeu :", "Game folder:"),
            ["Change"] = ("Changer...", "Change..."),
            ["ResetDefaults"] = ("Paramètres par défaut", "Restore defaults"),
            ["Save"] = ("Enregistrer", "Save"),
            ["Play"] = ("▶  Jouer", "▶  Play"),
            ["TabGraphics"] = ("Graphismes", "Graphics"),
            ["TabStability"] = ("Stabilité", "Stability"),
            ["TabSaves"] = ("Sauvegardes", "Saves"),
            ["TabInstall"] = ("Installation", "Installation"),
            ["Yes"] = ("Oui", "Yes"),
            ["No"] = ("Non", "No"),
            ["FolderPickerTitle"] = ("Dossier de Deadly Premonition (celui qui contient DP.exe)",
                                     "Deadly Premonition folder (the one that contains DP.exe)"),

            // --- Messages de la barre d'état
            ["GameNotFound"] = ("Jeu introuvable : choisissez le dossier qui contient DP.exe.",
                                "Game not found: choose the folder that contains DP.exe."),
            ["ExeNotFoundIn"] = ("DP.exe introuvable dans {0}.", "DP.exe not found in {0}."),
            ["LaunchedBySteam"] = ("Lancé par Steam : « Jouer » démarre le jeu.",
                                   "Started by Steam: “Play” starts the game."),
            ["ErrorPrefix"] = ("Erreur : {0}", "Error: {0}"),
            ["SettingsSaved"] = ("Réglages enregistrés : ils s'appliquent au prochain lancement du jeu.",
                                 "Settings saved: they apply the next time the game starts."),
            ["ResetConfirm"] = ("Revenir aux paramètres par défaut ?\n\nL'anticrénelage et les effets graphiques sont " +
                                "désactivés, et toutes les options du mod reprennent leur valeur d'origine. Le mode " +
                                "d'affichage et la résolution sont conservés. Les sauvegardes ne sont pas touchées.",
                                "Restore the default settings?\n\nAnti-aliasing and graphics effects are turned off, " +
                                "and every mod option goes back to its original value. The display mode and " +
                                "resolution are kept. Your saves are not touched."),
            ["ResetDone"] = ("Paramètres par défaut rétablis : ils s'appliquent au prochain lancement du jeu.",
                             "Default settings restored: they apply the next time the game starts."),
            ["GameAlreadyRunning"] = ("Le jeu est déjà lancé.", "The game is already running."),
            ["GameLaunched"] = ("Jeu lancé.", "Game launched."),
            ["SteamCopied"] = ("Option de lancement copiée : collez-la dans Steam (Propriétés du jeu > Général).",
                               "Launch option copied: paste it into Steam (game Properties > General)."),

            // --- Onglet Graphismes
            ["SecDisplay"] = ("Affichage", "Display"),
            ["Mode"] = ("Mode", "Mode"),
            ["Resolution"] = ("Résolution", "Resolution"),
            ["SecAntiAliasing"] = ("Anticrénelage", "Anti-aliasing"),
            ["AntiAliasingHint"] = ("Le suréchantillonnage calcule l'image à une résolution plus élevée puis la " +
                                    "réduit : c'est l'anticrénelage le plus propre, sans flou ni traînées. Le DLSS " +
                                    "n'est pas possible avec ce jeu (DirectX 9, pas de vecteurs de mouvement).",
                                    "Supersampling renders the image at a higher resolution and then scales it " +
                                    "down: it is the cleanest anti-aliasing, with no blur or ghosting. DLSS is not " +
                                    "possible with this game (DirectX 9, no motion vectors)."),
            ["RenderResolution"] = ("Image calculée en {0} × {1}, affichée en {2} × {3}.",
                                    "Image rendered at {0} × {1}, displayed at {2} × {3}."),
            ["SecEffects"] = ("Effets", "Effects"),
            ["AmbientOcclusion"] = ("Occlusion ambiante (SSAO)", "Ambient occlusion (SSAO)"),
            ["ShadowResolution"] = ("Résolution des ombres", "Shadow resolution"),
            ["ReflectionResolution"] = ("Résolution des reflets", "Reflection resolution"),
            ["PreciseShadows"] = ("Ombres plus précises (moins de crénelage dans les ombres)",
                                  "More precise shadows (less aliasing in shadows)"),
            ["ImprovedDof"] = ("Profondeur de champ améliorée (moins de scintillement)",
                               "Improved depth of field (less flickering)"),
            ["Anisotropic"] = ("Filtrage anisotrope 16× (textures plus nettes au loin)",
                               "16× anisotropic filtering (sharper distant textures)"),
            ["DpfixCredit"] = ("Effets de DPfix (Durante), intégré et corrigé dans DPStabilityFix.",
                               "Effects from DPfix (Durante), integrated and fixed in DPStabilityFix."),
            ["ModeBorderless"] = ("Plein écran sans bordure (recommandé)", "Borderless fullscreen (recommended)"),
            ["ModeFullscreen"] = ("Plein écran exclusif", "Exclusive fullscreen"),
            ["ModeWindowed"] = ("Fenêtré", "Windowed"),
            ["AaOff"] = ("Désactivé", "Off"),
            ["AaSmaa"] = ("Standard : SMAA", "Standard: SMAA"),
            ["AaSuper2"] = ("Bonne : suréchantillonnage ×2,25 + SMAA", "Good: ×2.25 supersampling + SMAA"),
            ["AaSuper4"] = ("Très bonne : suréchantillonnage ×4 + SMAA (recommandé)",
                            "Very good: ×4 supersampling + SMAA (recommended)"),
            ["AaSuper9"] = ("Excellente : suréchantillonnage ×9 + SMAA (GPU puissant)",
                            "Excellent: ×9 supersampling + SMAA (powerful GPU)"),
            ["AaCustom"] = ("Personnalisée (réglée dans DPfix.ini)", "Custom (set in DPfix.ini)"),
            ["SsaoOff"] = ("Désactivée", "Off"),
            ["SsaoLow"] = ("Légère", "Light"),
            ["SsaoMedium"] = ("Moyenne", "Medium"),
            ["SsaoHigh"] = ("Forte", "Strong"),
            ["ScaleDefault"] = ("D'origine", "Original"),
            ["ScaleHigh"] = ("Élevée (×4)", "High (×4)"),
            ["ScaleVeryHigh"] = ("Très élevée (×8)", "Very high (×8)"),

            // --- Onglet Stabilité
            ["SecSaves"] = ("Sauvegardes", "Saves"),
            ["AtomicSave"] = ("Écriture protégée de la sauvegarde", "Protected save writing"),
            ["AtomicSaveHint"] = ("Un plantage pendant la sauvegarde ne peut plus corrompre dp.sav : l'ancienne reste intacte.",
                                  "A crash while saving can no longer corrupt dp.sav: the previous one stays intact."),
            ["BackupCount"] = ("Copies de secours conservées", "Backups kept"),
            ["SecController"] = ("Manette", "Controller"),
            ["SonyLayout"] = ("Manette PlayStation reconnue comme une manette Xbox",
                              "PlayStation controller recognized as an Xbox controller"),
            ["SonyLayoutHint"] = ("DualSense et DualShock 4 : sans cela, la caméra tourne seule et les boutons sont mélangés.",
                                  "DualSense and DualShock 4: without this, the camera spins on its own and the buttons are mixed up."),
            ["SwapTriggers"] = ("Inverser L2 et R2", "Swap L2 and R2"),
            ["BackgroundPolling"] = ("Lire la manette en arrière-plan", "Read the controller in the background"),
            ["BackgroundPollingHint"] = ("Supprime une saccade d'environ 65 ms toutes les 20 secondes.",
                                         "Removes a hitch of about 65 ms every 20 seconds."),
            ["SecGame"] = ("Jeu", "Game"),
            ["AimFix"] = ("Correctif de la visée restreinte", "Restricted aiming fix"),
            ["AimFixHint"] = ("Bug du portage PC : le réticule atteint le bord de l'écran mais la caméra ne suit plus. Découvert par ZachFix.",
                              "PC port bug: the reticle reaches the screen edge but the camera stops following. Discovered by ZachFix."),
            ["SecSmoothness"] = ("Fluidité", "Smoothness"),
            ["PreciseGameTime"] = ("Temps du jeu en pleine précision", "Full-precision game time"),
            ["PreciseGameTimeHint"] = ("Sans cela, le temps calculé par le jeu perd en finesse quand le PC reste allumé " +
                                       "longtemps (démarrage rapide de Windows compris) : jusqu'à 65 ms de saccades après une semaine.",
                                       "Without this, the time computed by the game loses precision when the PC stays on " +
                                       "for a long time (Windows fast startup included): hitches of up to 65 ms after a week."),
            ["PreciseTimer"] = ("Minuteur Windows précis (1 ms)", "Precise Windows timer (1 ms)"),
            ["PreciseTimerHint"] = ("Les attentes du jeu sont sinon arrondies à environ 16 ms par Windows.",
                                    "Otherwise Windows rounds the game's waits to about 16 ms."),
            ["FrameLimit"] = ("Limiteur d'images (0 = désactivé, 60 conseillé)", "Frame limiter (0 = off, 60 recommended)"),
            ["SecDiagnostics"] = ("Plantages et diagnostic", "Crashes and diagnostics"),
            ["CrashDumps"] = ("Fichier de diagnostic à chaque plantage", "Diagnostic file on every crash"),
            ["FullDumps"] = ("Diagnostic complet (fichiers jusqu'à 2 Go, plus d'informations)",
                             "Full diagnostics (files up to 2 GB, more information)"),
            ["FrameStats"] = ("Mesures de fluidité dans le journal", "Smoothness measurements in the log"),
            ["SecGraphicsMod"] = ("Graphismes", "Graphics"),
            ["IntegratedDpfix"] = ("Utiliser DPfix intégré", "Use the integrated DPfix"),
            ["IntegratedDpfixHint"] = ("Désactivé automatiquement si un DPfix d'origine (d3d9.dll) est présent.",
                                       "Automatically disabled if an original DPfix (d3d9.dll) is present."),

            // --- Onglet Sauvegardes
            ["RestoreSelected"] = ("Restaurer la copie sélectionnée", "Restore the selected backup"),
            ["CurrentSave"] = ("Sauvegarde actuelle : {0}, {1} Ko", "Current save: {0}, {1} KB"),
            ["NoSave"] = ("Pas encore de sauvegarde (savedata\\dp.sav absent).", "No save yet (savedata\\dp.sav not found)."),
            ["BackupSize"] = ("{0} Ko", "{0} KB"),
            ["BackupStart"] = ("Lancement du jeu", "Game launch"),
            ["BackupBeforeWrite"] = ("Avant une sauvegarde", "Before a save"),
            ["BackupBeforeDelete"] = ("Avant une suppression", "Before a deletion"),
            ["BackupBeforeRestore"] = ("Avant une restauration", "Before a restore"),
            ["BackupInterruptedWrite"] = ("Écriture interrompue (incomplète)", "Interrupted write (incomplete)"),
            ["BackupInterruptedCheckpoint"] = ("Point de contrôle interrompu (incomplet)", "Interrupted checkpoint (incomplete)"),
            ["RestoreConfirm"] = ("Remplacer la sauvegarde actuelle par celle du {0} ({1}) ?\n\nLa sauvegarde actuelle " +
                                  "sera d'abord mise de côté dans les copies de secours.{2}",
                                  "Replace the current save with the one from {0} ({1})?\n\nThe current save will " +
                                  "first be set aside in the backups.{2}"),
            ["RestoreIncompleteWarning"] = ("\n\nAttention : cette copie vient d'une écriture interrompue, elle est probablement incomplète.",
                                            "\n\nWarning: this backup comes from an interrupted write, it is probably incomplete."),
            ["Restored"] = ("Sauvegarde du {0} restaurée.", "Save from {0} restored."),
            ["GameRunningBeforeRestore"] = ("Le jeu est lancé : fermez-le avant de restaurer une sauvegarde.",
                                            "The game is running: close it before restoring a save."),

            // --- Onglet Installation
            ["SecStatus"] = ("État", "Status"),
            ["SecInstallation"] = ("Installation", "Installation"),
            ["InstallHint"] = ("Patch 4 Go (copie de DP.exe d'origine conservée), mod, shaders. Vos réglages existants " +
                               "sont conservés. Disponible quand le launcher est lancé depuis le paquet téléchargé.",
                               "4 GB patch (a copy of the original DP.exe is kept), mod, shaders. Your existing " +
                               "settings are kept. Available when the launcher is run from the downloaded package."),
            ["InstallButton"] = ("Installer / mettre à jour", "Install / update"),
            ["InstallButtonVersion"] = ("Installer / mettre à jour (version {0})", "Install / update (version {0})"),
            ["DisableExternalDpfix"] = ("Désactiver DPfix d'origine", "Disable the original DPfix"),
            ["RestoreExternalDpfix"] = ("Réactiver DPfix d'origine", "Re-enable the original DPfix"),
            ["Uninstall"] = ("Désinstaller", "Uninstall"),
            ["SecSteam"] = ("Lancer depuis Steam", "Launch from Steam"),
            ["SteamHint"] = ("Pour ouvrir ce launcher en cliquant sur « Jouer » dans Steam : Propriétés du jeu > Général > " +
                             "Options de lancement, puis coller :",
                             "To open this launcher when you click “Play” in Steam: game Properties > General > " +
                             "Launch Options, then paste:"),
            ["Copy"] = ("Copier", "Copy"),
            ["SecDiagnosticTools"] = ("Diagnostic", "Diagnostics"),
            ["OpenLatestLog"] = ("Ouvrir le dernier journal", "Open the latest log"),
            ["OpenModFolder"] = ("Ouvrir le dossier du mod", "Open the mod folder"),
            ["CrashReportHint"] = ("En cas de plantage, joindre le dernier journal et le fichier .dmp du dossier crashdumps.",
                                   "After a crash, attach the latest log and the .dmp file from the crashdumps folder."),
            ["ExeUnreadable"] = ("DP.exe illisible", "DP.exe unreadable"),
            ["ExeKnownVersion"] = ("DP.exe : version Steam 1.01b ✓", "DP.exe: Steam version 1.01b ✓"),
            ["ExeUnknownVersion"] = ("DP.exe : version inconnue (horodatage 0x{0}), le mod a été conçu pour la 1.01b",
                                     "DP.exe: unknown version (timestamp 0x{0}), the mod was designed for 1.01b"),
            ["ModInstalledState"] = ("Mod installé : version {0} ✓", "Mod installed: version {0} ✓"),
            ["ModNotInstalled"] = ("Mod non installé", "Mod not installed"),
            ["LaaApplied"] = ("Patch 4 Go appliqué ✓", "4 GB patch applied ✓"),
            ["LaaMissing"] = ("Patch 4 Go non appliqué (le jeu est limité à 2 Go)", "4 GB patch not applied (the game is limited to 2 GB)"),
            ["DpfixExternalPresent"] = ("DPfix d'origine (d3d9.dll) présent : la version intégrée et corrigée est inactive.",
                                        "Original DPfix (d3d9.dll) present: the integrated, fixed version is inactive."),
            ["DpfixReady"] = ("DPfix intégré prêt ✓", "Integrated DPfix ready ✓"),
            ["DpfixShadersMissing"] = ("Shaders de DPfix absents : lancer l'installation.", "DPfix shaders missing: run the installation."),
            ["ExternalDpfixDisabled"] = ("DPfix d'origine désactivé (d3d9.dll renommé, pas supprimé).",
                                         "Original DPfix disabled (d3d9.dll renamed, not deleted)."),
            ["ExternalDpfixRestored"] = ("DPfix d'origine réactivé : la version intégrée se désactive.",
                                         "Original DPfix re-enabled: the integrated version turns itself off."),
            ["UninstallConfirm"] = ("Désinstaller DPStabilityFix ?\n\nLa DLL du mod est retirée et DP.exe d'origine " +
                                    "restauré. Vos réglages, journaux et copies de secours sont conservés.",
                                    "Uninstall DPStabilityFix?\n\nThe mod DLL is removed and the original DP.exe is " +
                                    "restored. Your settings, logs and backups are kept."),
            ["NoLogYet"] = ("Aucun journal pour l'instant : lancez le jeu une fois.", "No log yet: run the game once."),
            ["LogOpened"] = ("Journal ouvert : {0}", "Log opened: {0}"),

            // --- Installation (rapports de ModInstaller, aussi affichés par la ligne de commande)
            ["InstallInvalidExe"] = ("DP.exe introuvable, illisible, ou pas l'exécutable 32 bits attendu.",
                                     "DP.exe not found, unreadable, or not the expected 32-bit executable."),
            ["InstallUnknownVersion"] = ("Attention : ce DP.exe n'est pas la version Steam 1.01b pour laquelle le mod a été conçu.",
                                         "Warning: this DP.exe is not the Steam 1.01b version the mod was designed for."),
            ["InstallLaaApplied"] = ("Patch 4 Go appliqué (original conservé : DP.exe.dpsf-original).",
                                     "4 GB patch applied (original kept: DP.exe.dpsf-original)."),
            ["InstallLaaAlready"] = ("Patch 4 Go : déjà appliqué.", "4 GB patch: already applied."),
            ["InstallDllMissing"] = ("X3DAudio1_7.dll du mod introuvable dans le paquet.",
                                     "The mod's X3DAudio1_7.dll was not found in the package."),
            ["InstallDllKept"] = ("X3DAudio1_7.dll existante conservée sous X3DAudio1_7.dll.avant-dpsf.",
                                  "Existing X3DAudio1_7.dll kept as X3DAudio1_7.dll.avant-dpsf."),
            ["InstallModInstalled"] = ("Mod installé (version {0}).", "Mod installed (version {0})."),
            ["InstallFileKept"] = ("{0} existant conservé.", "Existing {0} kept."),
            ["InstallFileInstalled"] = ("{0} installé.", "{0} installed."),
            ["InstallShaders"] = ("Shaders de DPfix installés ({0} fichiers).", "DPfix shaders installed ({0} files)."),
            ["InstallLauncherCopied"] = ("Launcher copié dans le dossier du jeu.", "Launcher copied to the game folder."),
            ["UninstallDllRemoved"] = ("X3DAudio1_7.dll du mod retirée.", "The mod's X3DAudio1_7.dll was removed."),
            ["UninstallDllRestored"] = ("X3DAudio1_7.dll d'origine restaurée.", "Original X3DAudio1_7.dll restored."),
            ["UninstallExeRestored"] = ("DP.exe d'origine restauré (patch 4 Go retiré).", "Original DP.exe restored (4 GB patch removed)."),
            ["UninstallKept"] = ("Réglages, journaux et copies de secours conservés (dossier DPStabilityFix).",
                                 "Settings, logs and backups kept (DPStabilityFix folder)."),
            ["GameRunningClose"] = ("Le jeu est lancé : fermez-le d'abord.", "The game is running: close it first."),
            ["ExeInvalidHeader"] = ("DP.exe n'a pas un en-tête d'exécutable valide.", "DP.exe does not have a valid executable header."),
            ["PatchVerifyFailed"] = ("Patch 4 Go : vérification échouée après écriture.", "4 GB patch: verification failed after writing."),

            // --- Ligne de commande
            ["CliUsage"] = ("Usage : DPStabilityFix.exe install \"<DP.exe ou dossier du jeu>\" [--disable-external-dpfix]",
                            "Usage: DPStabilityFix.exe install \"<DP.exe or game folder>\" [--disable-external-dpfix]"),
            ["CliNotDpExe"] = ("Ce n'est pas DP.exe : {0}", "This is not DP.exe: {0}"),
            ["CliExternalDpfixDisabled"] = ("DPfix d'origine désactivé (d3d9.dll renommé en d3d9.dll.dpfix-desactive).",
                                            "Original DPfix disabled (d3d9.dll renamed to d3d9.dll.dpfix-desactive)."),
            ["CliFailure"] = ("Échec : {0}", "Failed: {0}"),
        };
}
