using System.Globalization;
using System.Text.RegularExpressions;

namespace DPStabilityFix.Launcher.Core;

/// <summary>Une copie de secours de dp.sav faite par la DLL (dossier DPStabilityFix\savebackups).</summary>
public sealed record SaveBackup(string Path, DateTime CreatedAt, long Size, string Reason)
{
    public string Description => Reason switch
    {
        "demarrage" => Loc.Get("BackupStart"),
        "avant-ecriture" => Loc.Get("BackupBeforeWrite"),
        "avant-suppression" => Loc.Get("BackupBeforeDelete"),
        "avant-restauration" => Loc.Get("BackupBeforeRestore"),
        "ecriture-interrompue" => Loc.Get("BackupInterruptedWrite"),
        "checkpoint-interrompu" => Loc.Get("BackupInterruptedCheckpoint"),
        _ => Reason,
    };

    /// <summary>Fichier récupéré après une écriture interrompue : probablement incomplet.</summary>
    public bool IsIncomplete => Reason.EndsWith("interrompue", StringComparison.Ordinal) ||
                                Reason.EndsWith("interrompu", StringComparison.Ordinal);
}

public static partial class SaveBackups
{
    // dp_20261003-140501-123_avant-ecriture.sav ou recovered_20261003-140501-123_ecriture-interrompue.sav
    [GeneratedRegex(@"^(?:dp|recovered)_(\d{8}-\d{6}-\d{3})_([a-z\-]+)\.sav$", RegexOptions.IgnoreCase)]
    private static partial Regex BackupName();

    public static IReadOnlyList<SaveBackup> List(GamePaths paths)
    {
        if (!Directory.Exists(paths.SaveBackupsDirectory))
        {
            return [];
        }
        List<SaveBackup> backups = new();
        foreach (string file in Directory.GetFiles(paths.SaveBackupsDirectory, "*.sav"))
        {
            Match match = BackupName().Match(System.IO.Path.GetFileName(file));
            if (!match.Success ||
                !DateTime.TryParseExact(match.Groups[1].Value, "yyyyMMdd-HHmmss-fff", CultureInfo.InvariantCulture,
                                        DateTimeStyles.AssumeLocal, out DateTime createdAt))
            {
                continue;
            }
            backups.Add(new SaveBackup(file, createdAt, new FileInfo(file).Length, match.Groups[2].Value));
        }
        return backups.OrderByDescending(backup => backup.CreatedAt).ToList();
    }

    /// <summary>
    /// Remplace dp.sav par une copie de secours. La sauvegarde actuelle est d'abord mise de côté
    /// (dp_..._avant-restauration.sav), pour pouvoir revenir en arrière.
    /// </summary>
    public static void Restore(GamePaths paths, SaveBackup backup)
    {
        if (GameProcess.IsRunning(paths))
        {
            throw new InvalidOperationException(Loc.Get("GameRunningBeforeRestore"));
        }
        Directory.CreateDirectory(paths.SaveBackupsDirectory);
        if (File.Exists(paths.SaveFile))
        {
            string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss-fff", CultureInfo.InvariantCulture);
            File.Copy(paths.SaveFile,
                      System.IO.Path.Combine(paths.SaveBackupsDirectory, $"dp_{stamp}_avant-restauration.sav"));
        }
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(paths.SaveFile)!);
        string temporary = paths.SaveFile + ".launcher-tmp";
        File.Copy(backup.Path, temporary, overwrite: true);
        File.Move(temporary, paths.SaveFile, overwrite: true);
    }
}
