using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Onglet Installation : état, installation / mise à jour, DPfix d'origine, désinstallation, diagnostic.</summary>
public sealed partial class StatusViewModel(MainWindowViewModel main, string launcherDirectory) : ObservableObject
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

    public string InstallButtonText => IsPackage ? $"Installer / mettre à jour (version {PackageVersion})" : "Installer / mettre à jour";

    public string SteamLaunchOption =>
        main.Paths is { } paths ? $"\"{Path.Combine(paths.GameDirectory, "DPStabilityFix.exe")}\" %command%" : string.Empty;

    public void Load(GamePaths paths)
    {
        InstallStatus status = ModInstaller.ReadStatus(paths);
        GameVersion = !status.Exe.IsValid ? "DP.exe illisible"
            : status.Exe.IsKnownVersion ? "DP.exe : version Steam 1.01b ✓"
            : $"DP.exe : version inconnue (horodatage 0x{status.Exe.Timestamp:X8}), le mod a été conçu pour la 1.01b";
        ModState = status.IsModInstalled ? $"Mod installé : version {status.InstalledModVersion} ✓" : "Mod non installé";
        MemoryState = status.Exe.LargeAddressAware ? "Patch 4 Go appliqué ✓" : "Patch 4 Go non appliqué (le jeu est limité à 2 Go)";
        HasExternalDpfix = status.HasExternalDpfix;
        HasDisabledExternalDpfix = status.HasDisabledExternalDpfix;
        DpfixState = status.HasExternalDpfix
            ? "DPfix d'origine (d3d9.dll) présent : la version intégrée et corrigée est inactive."
            : status.HasShaders ? "DPfix intégré prêt ✓" : "Shaders de DPfix absents : lancer l'installation.";
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
        return "DPfix d'origine désactivé (d3d9.dll renommé, pas supprimé).";
    });

    private bool CanRestoreExternalDpfix() => HasDisabledExternalDpfix && !HasExternalDpfix;

    [RelayCommand(CanExecute = nameof(CanRestoreExternalDpfix))]
    private void RestoreExternalDpfix() => main.Run(() =>
    {
        ModInstaller.RestoreExternalDpfix(main.Paths!);
        main.Reload();
        return "DPfix d'origine réactivé : la version intégrée se désactive.";
    });

    [RelayCommand]
    private async Task UninstallAsync()
    {
        if (!await main.ConfirmAsync("Désinstaller DPStabilityFix ?\n\nLa DLL du mod est retirée et DP.exe d'origine " +
                                     "restauré. Vos réglages, journaux et copies de secours sont conservés."))
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
            return "Aucun journal pour l'instant : lancez le jeu une fois.";
        }
        GameProcess.Open(latest);
        return $"Journal ouvert : {Path.GetFileName(latest)}";
    });

    [RelayCommand]
    private void OpenModFolder() => main.Run(() =>
    {
        Directory.CreateDirectory(main.Paths!.ModDataDirectory);
        GameProcess.Open(main.Paths.ModDataDirectory);
        return string.Empty;
    });
}
