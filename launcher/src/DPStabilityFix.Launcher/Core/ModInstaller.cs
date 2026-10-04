using System.Diagnostics;

namespace DPStabilityFix.Launcher.Core;

/// <summary>État de l'installation du mod dans un dossier de jeu.</summary>
public sealed record InstallStatus(
    ExeInfo Exe,
    bool HasOriginalExeBackup,
    string? InstalledModVersion,
    bool HasExternalDpfix,
    bool HasDisabledExternalDpfix,
    bool HasShaders)
{
    public bool IsModInstalled => InstalledModVersion is not null;
}

/// <summary>
/// Installation, mise à jour et désinstallation de DPStabilityFix. Les fichiers du mod sont pris dans le
/// dossier du paquet (celui du launcher) ; les réglages existants de l'utilisateur ne sont jamais écrasés.
/// </summary>
public static class ModInstaller
{
    private static readonly string[] SettingsFiles = ["DPStabilityFix.ini", "DPfix.ini", "DPfixKeys.ini"];

    public static InstallStatus ReadStatus(GamePaths paths) => new(
        ExePatcher.ReadInfo(paths.GameExe),
        File.Exists(paths.OriginalExeBackup),
        ModVersionOf(paths.ModDll),
        File.Exists(paths.ExternalDpfixDll),
        File.Exists(paths.DisabledExternalDpfixDll),
        File.Exists(Path.Combine(paths.ShaderDirectory, "SMAA.fx")));

    /// <summary>Le dossier contient-il les fichiers du mod à installer (paquet) ?</summary>
    public static bool IsPackageDirectory(string directory) =>
        ModVersionOf(Path.Combine(directory, GamePaths.ModDllName)) is not null &&
        Directory.Exists(Path.Combine(directory, "dpfix"));

    /// <summary>Version de notre X3DAudio1_7.dll (null si absente ou si c'est une autre DLL).</summary>
    public static string? ModVersionOf(string dllPath)
    {
        if (!File.Exists(dllPath))
        {
            return null;
        }
        FileVersionInfo info = FileVersionInfo.GetVersionInfo(dllPath);
        return info.FileDescription?.Contains("DPStabilityFix", StringComparison.Ordinal) == true
            ? info.FileVersion
            : null;
    }

    /// <summary>
    /// Installe ou met à jour : patch 4 Go, DLL, shaders, réglages absents, et copie du launcher dans le
    /// dossier du jeu. Renvoie le compte rendu des actions.
    /// </summary>
    public static IReadOnlyList<string> Install(string packageDirectory, GamePaths paths, string? launcherExecutable)
    {
        List<string> report = new();
        ExeInfo exe = ExePatcher.ReadInfo(paths.GameExe);
        if (!exe.IsValid || !exe.IsX86)
        {
            throw new InvalidDataException("DP.exe introuvable, illisible, ou pas l'exécutable 32 bits attendu.");
        }
        EnsureGameNotRunning(paths);
        if (!exe.IsKnownVersion)
        {
            report.Add("Attention : ce DP.exe n'est pas la version Steam 1.01b pour laquelle le mod a été conçu.");
        }

        report.Add(ExePatcher.EnableLargeAddressAware(paths.GameExe)
            ? "Patch 4 Go appliqué (original conservé : DP.exe.dpsf-original)."
            : "Patch 4 Go : déjà appliqué.");

        string sourceDll = Path.Combine(packageDirectory, GamePaths.ModDllName);
        if (ModVersionOf(sourceDll) is not { } version)
        {
            throw new FileNotFoundException("X3DAudio1_7.dll du mod introuvable dans le paquet.", sourceDll);
        }
        if (File.Exists(paths.ModDll) && ModVersionOf(paths.ModDll) is null)
        {
            File.Copy(paths.ModDll, paths.ModDll + ".avant-dpsf", overwrite: true);
            report.Add("X3DAudio1_7.dll existante conservée sous X3DAudio1_7.dll.avant-dpsf.");
        }
        File.Copy(sourceDll, paths.ModDll, overwrite: true);
        report.Add($"Mod installé (version {version}).");

        foreach (string name in SettingsFiles)
        {
            string target = Path.Combine(paths.GameDirectory, name);
            string source = Path.Combine(packageDirectory, name);
            if (File.Exists(target))
            {
                report.Add($"{name} existant conservé.");
            }
            else if (File.Exists(source))
            {
                File.Copy(source, target);
                report.Add($"{name} installé.");
            }
        }

        string sourceShaders = Path.Combine(packageDirectory, "dpfix");
        Directory.CreateDirectory(paths.ShaderDirectory);
        int shaderCount = 0;
        foreach (string file in Directory.GetFiles(sourceShaders))
        {
            File.Copy(file, Path.Combine(paths.ShaderDirectory, Path.GetFileName(file)), overwrite: true);
            shaderCount++;
        }
        report.Add($"Shaders de DPfix installés ({shaderCount} fichiers).");

        string license = Path.Combine(packageDirectory, "LICENSE.txt");
        if (File.Exists(license))
        {
            File.Copy(license, Path.Combine(paths.GameDirectory, "DPStabilityFix-LICENSE.txt"), overwrite: true);
        }

        // Le launcher est aussi copié à côté du jeu, pour pouvoir être utilisé comme option de lancement Steam.
        if (launcherExecutable is not null && File.Exists(launcherExecutable))
        {
            string target = Path.Combine(paths.GameDirectory, Path.GetFileName(launcherExecutable));
            if (!string.Equals(Path.GetFullPath(launcherExecutable), Path.GetFullPath(target),
                               StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(launcherExecutable, target, overwrite: true);
                report.Add("Launcher copié dans le dossier du jeu.");
            }
        }
        return report;
    }

    /// <summary>Renomme d3d9.dll (DPfix d'origine) sans le supprimer, pour laisser la place à DPfix intégré.</summary>
    public static void DisableExternalDpfix(GamePaths paths)
    {
        EnsureGameNotRunning(paths);
        File.Move(paths.ExternalDpfixDll, paths.DisabledExternalDpfixDll, overwrite: true);
    }

    public static void RestoreExternalDpfix(GamePaths paths)
    {
        EnsureGameNotRunning(paths);
        File.Move(paths.DisabledExternalDpfixDll, paths.ExternalDpfixDll, overwrite: false);
    }

    /// <summary>
    /// Retire le mod : DLL supprimée (une X3DAudio1_7.dll d'origine mise de côté est remise), DP.exe
    /// d'origine restauré. Les réglages, journaux et copies de secours sont conservés.
    /// </summary>
    public static IReadOnlyList<string> Uninstall(GamePaths paths)
    {
        List<string> report = new();
        EnsureGameNotRunning(paths);
        if (File.Exists(paths.ModDll) && ModVersionOf(paths.ModDll) is not null)
        {
            File.Delete(paths.ModDll);
            report.Add("X3DAudio1_7.dll du mod retirée.");
        }
        if (File.Exists(paths.ModDll + ".avant-dpsf"))
        {
            File.Move(paths.ModDll + ".avant-dpsf", paths.ModDll, overwrite: true);
            report.Add("X3DAudio1_7.dll d'origine restaurée.");
        }
        if (File.Exists(paths.OriginalExeBackup))
        {
            ExePatcher.RestoreOriginal(paths.GameExe);
            report.Add("DP.exe d'origine restauré (patch 4 Go retiré).");
        }
        report.Add("Réglages, journaux et copies de secours conservés (dossier DPStabilityFix).");
        return report;
    }

    private static void EnsureGameNotRunning(GamePaths paths)
    {
        if (GameProcess.IsRunning(paths) || IsLocked(paths.GameExe))
        {
            throw new InvalidOperationException("Le jeu est lancé : fermez-le d'abord.");
        }
    }

    // DP.exe ouvert par un autre programme (jeu lancé autrement, outil de modding) : ne pas le modifier.
    private static bool IsLocked(string path)
    {
        if (!File.Exists(path))
        {
            return false;
        }
        try
        {
            using FileStream stream = new(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None);
            return false;
        }
        catch (IOException)
        {
            return true;
        }
    }
}
