namespace DPStabilityFix.Launcher.Core;

/// <summary>Tous les chemins utiles d'une installation du jeu, calculés depuis son dossier.</summary>
public sealed record GamePaths(string GameDirectory)
{
    public const string GameExeName = "DP.exe";
    public const string ModDllName = "X3DAudio1_7.dll";

    public string GameExe => Path.Combine(GameDirectory, GameExeName);
    public string OriginalExeBackup => GameExe + ".dpsf-original";
    public string ModDll => Path.Combine(GameDirectory, ModDllName);
    public string ModIni => Path.Combine(GameDirectory, "DPStabilityFix.ini");
    public string DpfixIni => Path.Combine(GameDirectory, "DPfix.ini");
    public string DpfixKeysIni => Path.Combine(GameDirectory, "DPfixKeys.ini");
    public string ShaderDirectory => Path.Combine(GameDirectory, "dpfix");

    /// <summary>d3d9.dll de DPfix d'origine (Durante), et son nom une fois désactivé.</summary>
    public string ExternalDpfixDll => Path.Combine(GameDirectory, "d3d9.dll");
    public string DisabledExternalDpfixDll => ExternalDpfixDll + ".dpfix-desactive";

    public string ModDataDirectory => Path.Combine(GameDirectory, "DPStabilityFix");
    public string LogsDirectory => Path.Combine(ModDataDirectory, "logs");
    public string CrashDumpsDirectory => Path.Combine(ModDataDirectory, "crashdumps");
    public string SaveBackupsDirectory => Path.Combine(ModDataDirectory, "savebackups");
    public string SaveFile => Path.Combine(GameDirectory, "savedata", "dp.sav");

    public bool ContainsGame => File.Exists(GameExe);
}
