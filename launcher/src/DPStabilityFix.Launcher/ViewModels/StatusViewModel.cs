using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Onglet Installation : état, installation / mise à jour, DPfix d'origine, désinstallation, diagnostic.</summary>
public sealed partial class StatusViewModel(MainWindowViewModel main, string launcherDirectory) : LocalizedViewModel
{
    public bool IsPackage { get; } = ModInstaller.IsPackageDirectory(launcherDirectory);

    public string PackageVersion =>
        ModInstaller.ModVersionOf(Path.Combine(launcherDirectory, GamePaths.ModDllName)) ?? "?";

    [ObservableProperty]
    public partial string GameVersion { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string ModState { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string MemoryState { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string DpfixState { get; set; } = string.Empty;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(DisableExternalDpfixCommand))]
    public partial bool HasExternalDpfix { get; set; }

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(RestoreExternalDpfixCommand))]
    public partial bool HasDisabledExternalDpfix { get; set; }

    public string InstallButtonText => IsPackage ? Loc.Get("InstallButtonVersion", PackageVersion) : Loc.Get("InstallButton");

    public string SteamLaunchOption =>
        main.Paths is { } paths ? $"\"{Path.Combine(paths.GameDirectory, "DPStabilityFix.exe")}\" %command%" : string.Empty;

    /// <summary>Après un changement de langue : le texte du bouton d'installation est recalculé.</summary>
    public void NotifyLanguageChanged() => OnPropertyChanged(nameof(InstallButtonText));

    public void Load(GamePaths paths)
    {
        InstallStatus status = ModInstaller.ReadStatus(paths);
        GameVersion = !status.Exe.IsValid ? Loc.Get("ExeUnreadable")
            : status.Exe.IsKnownVersion ? Loc.Get("ExeKnownVersion")
            : Loc.Get("ExeUnknownVersion", status.Exe.Timestamp.ToString("X8"));
        ModState = status.IsModInstalled ? Loc.Get("ModInstalledState", status.InstalledModVersion ?? "?") : Loc.Get("ModNotInstalled");
        MemoryState = status.Exe.LargeAddressAware ? Loc.Get("LaaApplied") : Loc.Get("LaaMissing");
        HasExternalDpfix = status.HasExternalDpfix;
        HasDisabledExternalDpfix = status.HasDisabledExternalDpfix;
        DpfixState = status.HasExternalDpfix
            ? Loc.Get("DpfixExternalPresent")
            : status.HasShaders ? Loc.Get("DpfixReady") : Loc.Get("DpfixShadersMissing");
        OnPropertyChanged(nameof(SteamLaunchOption));
    }

    private bool CanInstall() => IsPackage;

    [RelayCommand(CanExecute = nameof(CanInstall))]
    private void Install() => main.Run(() =>
    {
        IReadOnlyList<string> report = ModInstaller.Install(launcherDirectory, main.Paths!, Environment.ProcessPath);
        main.Reload();
        return string.Join(" ", report);
    });

    private bool CanDisableExternalDpfix() => HasExternalDpfix;

    [RelayCommand(CanExecute = nameof(CanDisableExternalDpfix))]
    private void DisableExternalDpfix() => main.Run(() =>
    {
        ModInstaller.DisableExternalDpfix(main.Paths!);
        main.Reload();
        return Loc.Get("ExternalDpfixDisabled");
    });

    private bool CanRestoreExternalDpfix() => HasDisabledExternalDpfix && !HasExternalDpfix;

    [RelayCommand(CanExecute = nameof(CanRestoreExternalDpfix))]
    private void RestoreExternalDpfix() => main.Run(() =>
    {
        ModInstaller.RestoreExternalDpfix(main.Paths!);
        main.Reload();
        return Loc.Get("ExternalDpfixRestored");
    });

    [RelayCommand]
    private async Task UninstallAsync()
    {
        if (!await main.ConfirmAsync(Loc.Get("UninstallConfirm")))
        {
            return;
        }
        main.Run(() =>
        {
            IReadOnlyList<string> report = ModInstaller.Uninstall(main.Paths!);
            main.Reload();
            return string.Join(" ", report);
        });
    }

    [RelayCommand]
    private void OpenLatestLog() => main.Run(() =>
    {
        string? latest = Directory.Exists(main.Paths!.LogsDirectory)
            ? Directory.GetFiles(main.Paths.LogsDirectory, "*.log").OrderDescending().FirstOrDefault()
            : null;
        if (latest is null)
        {
            return Loc.Get("NoLogYet");
        }
        GameProcess.Open(latest);
        return Loc.Get("LogOpened", Path.GetFileName(latest));
    });

    [RelayCommand]
    private void OpenModFolder() => main.Run(() =>
    {
        Directory.CreateDirectory(main.Paths!.ModDataDirectory);
        GameProcess.Open(main.Paths.ModDataDirectory);
        return string.Empty;
    });
}
