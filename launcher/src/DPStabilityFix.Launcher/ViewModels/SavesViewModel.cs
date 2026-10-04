using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Une ligne de la liste des copies de secours.</summary>
public sealed record SaveBackupItem(SaveBackup Backup)
{
    public string Date => Backup.CreatedAt.ToString("dd/MM/yyyy HH:mm:ss");
    public string Description => Backup.IsIncomplete ? $"{Backup.Description} ⚠" : Backup.Description;
    public string Size => $"{Backup.Size / 1024.0:N0} Ko";
}

/// <summary>Onglet Sauvegardes : copies de secours de dp.sav et restauration.</summary>
public sealed partial class SavesViewModel(MainWindowViewModel main) : ObservableObject
{
    public ObservableCollection<SaveBackupItem> Backups { get; } = new();

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(RestoreCommand))]
    public partial SaveBackupItem? SelectedBackup { get; set; }

    [ObservableProperty]
    public partial string CurrentSave { get; set; } = string.Empty;

    public void Load(GamePaths paths)
    {
        Backups.Clear();
        foreach (SaveBackup backup in SaveBackups.List(paths))
        {
            Backups.Add(new SaveBackupItem(backup));
        }
        CurrentSave = File.Exists(paths.SaveFile)
            ? $"Sauvegarde actuelle : {File.GetLastWriteTime(paths.SaveFile):dd/MM/yyyy HH:mm:ss}, {new FileInfo(paths.SaveFile).Length / 1024.0:N0} Ko"
            : "Pas encore de sauvegarde (savedata\\dp.sav absent).";
    }

    private bool CanRestore() => SelectedBackup is not null;

    [RelayCommand(CanExecute = nameof(CanRestore))]
    private async Task RestoreAsync()
    {
        if (SelectedBackup is not { } item || main.Paths is not { } paths)
        {
            return;
        }
        string warning = item.Backup.IsIncomplete
            ? "\n\nAttention : cette copie vient d'une écriture interrompue, elle est probablement incomplète."
            : string.Empty;
        bool confirmed = await main.ConfirmAsync(
            $"Remplacer la sauvegarde actuelle par celle du {item.Date} ({item.Backup.Description}) ?\n\n" +
            $"La sauvegarde actuelle sera d'abord mise de côté dans les copies de secours.{warning}");
        if (!confirmed)
        {
            return;
        }
        main.Run(() =>
        {
            SaveBackups.Restore(paths, item.Backup);
            Load(paths);
            return $"Sauvegarde du {item.Date} restaurée.";
        });
    }
}
