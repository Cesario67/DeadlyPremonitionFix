using Avalonia.Controls;
using Avalonia.Input.Platform;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using DPStabilityFix.Launcher.Core;
using DPStabilityFix.Launcher.ViewModels;

namespace DPStabilityFix.Launcher.Views;

/// <summary>
/// Code de la vue limité à ce qui demande la fenêtre elle-même : sélecteur de dossier, presse-papiers,
/// boîte de confirmation, masquage pendant la partie. Toute la logique est dans les ViewModels.
/// </summary>
public sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        DataContextChanged += (_, _) =>
        {
            if (DataContext is MainWindowViewModel viewModel)
            {
                viewModel.ConfirmAsync = message => ConfirmDialog.ShowAsync(this, message);
                viewModel.WaitForGameThenExit = () => _ = HideUntilGameExitsAsync(viewModel);
            }
        };
    }

    private MainWindowViewModel? ViewModel => DataContext as MainWindowViewModel;

    private async void ChooseGameFolder(object? sender, RoutedEventArgs e)
    {
        IReadOnlyList<IStorageFolder> folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = Loc.Get("FolderPickerTitle"),
            AllowMultiple = false,
        });
        if (folders.Count > 0 && folders[0].TryGetLocalPath() is { } path)
        {
            ViewModel?.SetGameDirectory(path);
        }
    }

    private async void CopySteamLaunchOption(object? sender, RoutedEventArgs e)
    {
        if (ViewModel is { } viewModel && Clipboard is { } clipboard)
        {
            await clipboard.SetTextAsync(viewModel.Status.SteamLaunchOption);
            viewModel.Message = Loc.Get("SteamCopied");
        }
    }

    private async Task HideUntilGameExitsAsync(MainWindowViewModel viewModel)
    {
        Hide();
        await viewModel.WaitForGameAsync();
        Close();
    }
}
