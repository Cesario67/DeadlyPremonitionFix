using System.Runtime.Versioning;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// Retrouve le dossier du jeu : celui du launcher s'il contient DP.exe, puis le dernier dossier choisi
/// (mémorisé dans %APPDATA%\DPStabilityFix\game-directory.txt), puis les bibliothèques Steam.
/// </summary>
public static partial class GameLocator
{
    public const string SteamAppId = "247660";
    private const string GameFolderName = "Deadly Premonition The Director's Cut";

    // Fichier texte d'une ligne (le dossier du jeu) : pas de JSON, dont la sérialisation par réflexion est
    // incompatible avec l'élagage du .exe publié.
    private static string SettingsFile => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "DPStabilityFix", "game-directory.txt");

    public static string? FindGameDirectory(string launcherDirectory)
    {
        if (new GamePaths(launcherDirectory).ContainsGame)
        {
            return launcherDirectory;
        }
        if (LoadRememberedDirectory() is { } remembered && new GamePaths(remembered).ContainsGame)
        {
            return remembered;
        }
        if (OperatingSystem.IsWindows())
        {
            foreach (string library in SteamLibraries())
            {
                string candidate = Path.Combine(library, "steamapps", "common", GameFolderName);
                if (new GamePaths(candidate).ContainsGame)
                {
                    return candidate;
                }
            }
        }
        return null;
    }

    public static void RememberDirectory(string gameDirectory)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(SettingsFile)!);
        File.WriteAllText(SettingsFile, gameDirectory);
    }

    private static string? LoadRememberedDirectory() =>
        File.Exists(SettingsFile) ? File.ReadAllText(SettingsFile).Trim() : null;

    // libraryfolders.vdf contient une ligne "path" "D:\\SteamLibrary" par bibliothèque.
    [GeneratedRegex("\"path\"\\s+\"([^\"]+)\"")]
    private static partial Regex LibraryPath();

    [SupportedOSPlatform("windows")]
    private static IEnumerable<string> SteamLibraries()
    {
        if (Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) is not string steam)
        {
            yield break;
        }
        steam = steam.Replace('/', Path.DirectorySeparatorChar);
        yield return steam;
        string folders = Path.Combine(steam, "steamapps", "libraryfolders.vdf");
        if (!File.Exists(folders))
        {
            yield break;
        }
        foreach (Match match in LibraryPath().Matches(File.ReadAllText(folders)))
        {
            yield return match.Groups[1].Value.Replace(@"\\", @"\");
        }
    }
}
