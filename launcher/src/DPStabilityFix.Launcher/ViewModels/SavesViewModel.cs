using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Une ligne de la liste des copies de secours.</summary>
public sealed record SaveBackupItem(SaveBackup Backup)
{
    public string Date => Backup.CreatedAt.ToString(Loc.DateTimeFormat, Loc.Culture);
    public string Description => Backup.IsIncomplete ? $"{Backup.Description} ⚠" : Backup.Description;
    public string Size => Loc.Get("BackupSize", (Backup.Size / 1024.0).ToString("N0", Loc.Culture));
}

/// <summary>Onglet Sauvegardes : copies de secours de dp.sav et restauration.</summary>
public sealed partial class SavesViewModel(MainWindowViewModel main) : LocalizedViewModel
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
            ? Loc.Get("CurrentSave",
                      File.GetLastWriteTime(paths.SaveFile).ToString(Loc.DateTimeFormat, Loc.Culture),
                      (new FileInfo(paths.SaveFile).Length / 1024.0).ToString("N0", Loc.Culture))
            : Loc.Get("NoSave");
    }

    private bool CanRestore() => SelectedBackup is not null;

    [RelayCommand(CanExecute = nameof(CanRestore))]
    private async Task RestoreAsync()
    {
        if (SelectedBackup is not { } item || main.Paths is not { } paths)
        {
            return;
        }
        string warning = item.Backup.IsIncomplete ? Loc.Get("RestoreIncompleteWarning") : string.Empty;
        bool confirmed = await main.ConfirmAsync(Loc.Get("RestoreConfirm", item.Date, item.Backup.Description, warning));
        if (!confirmed)
        {
            return;
        }
        main.Run(() =>
        {
            SaveBackups.Restore(paths, item.Backup);
            Load(paths);
            return Loc.Get("Restored", item.Date);
        });
    }
}
