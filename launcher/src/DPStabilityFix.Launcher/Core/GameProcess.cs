using System.Diagnostics;

namespace DPStabilityFix.Launcher.Core;

/// <summary>Détection et lancement du jeu.</summary>
public static class GameProcess
{
    /// <summary>Vrai si DP.exe est en cours d'exécution depuis ce dossier.</summary>
    public static bool IsRunning(GamePaths paths)
    {
        foreach (Process process in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(GamePaths.GameExeName)))
        {
            using (process)
            {
                try
                {
                    if (string.Equals(process.MainModule?.FileName, paths.GameExe, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
                catch (Exception exception) when (exception is InvalidOperationException or System.ComponentModel.Win32Exception)
                {
                    // Processus d'un autre utilisateur ou terminé entre-temps : on considère qu'il peut s'agir du jeu.
                    return true;
                }
            }
        }
        return false;
    }

    /// <summary>
    /// Lance le jeu. Si le launcher a été démarré par Steam (option de lancement « ... %command% »), on
    /// exécute la commande d'origine de Steam ; sinon DP.exe directement (steam_appid.txt, présent dans le
    /// dossier du jeu, permet à Steam de reconnaître le jeu tant que le client Steam est ouvert).
    /// </summary>
    public static void Launch(GamePaths paths, IReadOnlyList<string> steamCommand)
    {
        ProcessStartInfo start;
        if (steamCommand.Count > 0)
        {
            start = new ProcessStartInfo(steamCommand[0]) { UseShellExecute = false };
            foreach (string argument in steamCommand.Skip(1))
            {
                start.ArgumentList.Add(argument);
            }
            start.WorkingDirectory = Path.GetDirectoryName(steamCommand[0]) ?? paths.GameDirectory;
        }
        else
        {
            start = new ProcessStartInfo(paths.GameExe)
            {
                UseShellExecute = false,
                WorkingDirectory = paths.GameDirectory,
            };
        }
        Process.Start(start)?.Dispose();
    }

    /// <summary>Ouvre un fichier ou un dossier avec l'application associée (Explorateur, éditeur de texte...).</summary>
    public static void Open(string path)
    {
        Process.Start(new ProcessStartInfo(path) { UseShellExecute = true })?.Dispose();
    }
}
