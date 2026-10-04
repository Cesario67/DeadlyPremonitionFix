using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Fenêtre principale : dossier du jeu, onglets, enregistrement des réglages et lancement.</summary>
public sealed partial class MainWindowViewModel : ObservableObject
{
    private readonly IReadOnlyList<string> _steamCommand;

    public MainWindowViewModel(string launcherDirectory, IReadOnlyList<string> steamCommand)
    {
        _steamCommand = steamCommand;
        Status = new StatusViewModel(this, launcherDirectory);
        Saves = new SavesViewModel(this);
        if (GameLocator.FindGameDirectory(launcherDirectory) is { } directory)
        {
            SetGameDirectory(directory);
        }
        else
        {
            Message = "Jeu introuvable : choisissez le dossier qui contient DP.exe.";
        }
    }

    public GraphicsViewModel Graphics { get; } = new();
    public StabilityViewModel Stability { get; } = new();
    public SavesViewModel Saves { get; }
    public StatusViewModel Status { get; }

    public GamePaths? Paths { get; private set; }

    /// <summary>Fourni par la vue : affiche une question oui/non.</summary>
    public Func<string, Task<bool>> ConfirmAsync { get; set; } = _ => Task.FromResult(false);

    /// <summary>
    /// Fourni par la vue, appelé quand le launcher a été démarré par Steam et vient de lancer le jeu : la
    /// fenêtre se cache et l'application attend la fin du jeu. Steam considère en effet que le jeu tourne tant
    /// que le programme qu'il a lancé (ce launcher) tourne : overlay et temps de jeu en dépendent.
    /// </summary>
    public Action WaitForGameThenExit { get; set; } = () => { };

    /// <summary>Attend que DP.exe démarre (jusqu'à une minute), puis qu'il se termine.</summary>
    public async Task WaitForGameAsync()
    {
        if (Paths is null)
        {
            return;
        }
        for (int attempt = 0; attempt < 60 && !GameProcess.IsRunning(Paths); attempt++)
        {
            await Task.Delay(TimeSpan.FromSeconds(1));
        }
        while (GameProcess.IsRunning(Paths))
        {
            await Task.Delay(TimeSpan.FromSeconds(2));
        }
    }

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(HasGame))]
    [NotifyCanExecuteChangedFor(nameof(PlayCommand), nameof(SaveSettingsCommand))]
    public partial string GameDirectory { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string Message { get; set; } = string.Empty;

    public bool HasGame => Paths is not null;

    public bool LaunchedBySteam => _steamCommand.Count > 0;

    /// <summary>Choisit un dossier de jeu (au démarrage ou depuis le bouton « Changer »).</summary>
    public void SetGameDirectory(string directory)
    {
        GamePaths paths = new(directory);
        if (!paths.ContainsGame)
        {
            Message = $"DP.exe introuvable dans {directory}.";
            return;
        }
        Paths = paths;
        GameLocator.RememberDirectory(directory);
        GameDirectory = directory;
        Reload();
        Message = LaunchedBySteam ? "Lancé par Steam : « Jouer » démarre le jeu." : string.Empty;
    }

    /// <summary>Relit l'état et les réglages depuis le disque.</summary>
    public void Reload()
    {
        if (Paths is null)
        {
            return;
        }
        Status.Load(Paths);
        Graphics.Load(DpfixConfig.Load(Paths.DpfixIni));
        Stability.Load(IniDocument.Load(Paths.ModIni));
        Saves.Load(Paths);
    }

    /// <summary>Exécute une action en affichant son résultat ou son erreur dans la barre d'état.</summary>
    public void Run(Func<string> action)
    {
        try
        {
            Message = action();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
                                              or InvalidOperationException or InvalidDataException
                                              or System.ComponentModel.Win32Exception)
        {
            Message = $"Erreur : {exception.Message}";
        }
    }

    private bool CanUseGame() => HasGame;

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private void SaveSettings() => Run(() =>
    {
        DpfixConfig dpfix = DpfixConfig.Load(Paths!.DpfixIni);
        Graphics.Save(dpfix);
        dpfix.Save(Paths.DpfixIni);
        IniDocument mod = IniDocument.Load(Paths.ModIni);
        Stability.Save(mod);
        mod.Save(Paths.ModIni);
        return "Réglages enregistrés : ils s'appliquent au prochain lancement du jeu.";
    });

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private void Play() => Run(() =>
    {
        if (GameProcess.IsRunning(Paths!))
        {
            return "Le jeu est déjà lancé.";
        }
        SaveSettings();
        GameProcess.Launch(Paths!, _steamCommand);
        if (LaunchedBySteam)
        {
            WaitForGameThenExit();
        }
        return "Jeu lancé.";
    });
}
