using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using DPStabilityFix.Launcher.ViewModels;
using DPStabilityFix.Launcher.Views;

namespace DPStabilityFix.Launcher;

public sealed class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainWindowViewModel(AppContext.BaseDirectory, Program.SteamCommand),
            };
        }
        base.OnFrameworkInitializationCompleted();
    }
}
