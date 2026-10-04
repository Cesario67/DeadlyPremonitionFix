namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// Remet les réglages du mod et de DPfix à leurs valeurs d'origine (celles des fichiers livrés avec le mod) :
/// anticrénelage et effets désactivés, options de stabilité par défaut. L'affichage choisi par le joueur
/// (mode et résolution) est conservé : il dépend de son écran, pas du mod. Les fichiers sont modifiés en
/// place, leurs commentaires et les clés inconnues du launcher sont conservés.
/// </summary>
public static class DefaultSettings
{
    public static void Apply(DpfixConfig dpfix, IniDocument mod)
    {
        GraphicsSettings current = GraphicsSettingsMapper.Read(dpfix);
        GraphicsSettings defaults = new()
        {
            DisplayMode = current.DisplayMode,
            DisplayWidth = current.DisplayWidth,
            DisplayHeight = current.DisplayHeight,
            AntiAliasing = AntiAliasing.Off,
        };
        GraphicsSettingsMapper.Write(defaults, dpfix);
        dpfix.SetInt("logLevel", 0);

        new StabilitySettings().Write(mod);
        // Options absentes du launcher, remises elles aussi à leur valeur par défaut.
        mod.SetBool("Frames", "ForceFpuPreserve", true);
        mod.SetBool("Gameplay", "SkipIntro", true);
        mod.SetBool("Controller", "Diagnostics", true);
    }
}
