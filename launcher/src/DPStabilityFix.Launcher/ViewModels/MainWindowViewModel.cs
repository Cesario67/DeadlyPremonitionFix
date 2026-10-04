using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Fenêtre principale : dossier du jeu, onglets, enregistrement des réglages et lancement.</summary>
public sealed partial class MainWindowViewModel : LocalizedViewModel
{
    private readonly IReadOnlyList<string> _steamCommand;

    /// <param name="launcherDirectory">Dossier du launcher : le jeu y est cherché en premier.</param>
    /// <param name="packageDirectory">Dossier des fichiers du mod à installer (voir <see cref="EmbeddedPackage"/>).</param>
    public MainWindowViewModel(string launcherDirectory, IReadOnlyList<string> steamCommand, string? packageDirectory = null)
    {
        _steamCommand = steamCommand;
        SelectedLanguage = Languages.First(choice => choice.Value == Loc.Current);
        Loc.Instance.LanguageChanged += OnLanguageChanged;
        Status = new StatusViewModel(this, packageDirectory ?? launcherDirectory);
        Saves = new SavesViewModel(this);
        if (GameLocator.FindGameDirectory(launcherDirectory) is { } directory)
        {
            SetGameDirectory(directory);
        }
        else
        {
            Message = Loc.Get("GameNotFound");
        }
    }

    /// <summary>Langues proposées (noms écrits dans leur propre langue, jamais traduits).</summary>
    public IReadOnlyList<Choice<Language>> Languages { get; } =
    [
        new(Language.French, "Français"),
        new(Language.English, "English"),
    ];

    [ObservableProperty]
    public partial Choice<Language> SelectedLanguage { get; set; }

    partial void OnSelectedLanguageChanged(Choice<Language> value) => Loc.SetLanguage(value.Value);

    /// <summary>
    /// Après un changement de langue : les textes lus sur le disque (état, sauvegardes) sont recalculés. Les
    /// réglages en cours d'édition ne sont pas relus, pour ne pas perdre les modifications non enregistrées.
    /// </summary>
    private void OnLanguageChanged()
    {
        Message = string.Empty;
        if (Paths is not null)
        {
            Status.Load(Paths);
            Saves.Load(Paths);
        }
        Status.NotifyLanguageChanged();
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
            Message = Loc.Get("ExeNotFoundIn", directory);
            return;
        }
        Paths = paths;
        GameLocator.RememberDirectory(directory);
        GameDirectory = directory;
        Reload();
        Message = LaunchedBySteam ? Loc.Get("LaunchedBySteam") : string.Empty;
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
            Message = Loc.Get("ErrorPrefix", exception.Message);
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
        return Loc.Get("SettingsSaved");
    });

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private async Task ResetSettingsAsync()
    {
        bool confirmed = await ConfirmAsync(Loc.Get("ResetConfirm"));
        if (!confirmed)
        {
            return;
        }
        Run(() =>
        {
            DpfixConfig dpfix = DpfixConfig.Load(Paths!.DpfixIni);
            IniDocument mod = IniDocument.Load(Paths.ModIni);
            DefaultSettings.Apply(dpfix, mod);
            dpfix.Save(Paths.DpfixIni);
            mod.Save(Paths.ModIni);
            Reload();
            return Loc.Get("ResetDone");
        });
    }

    [RelayCommand(CanExecute = nameof(CanUseGame))]
    private void Play() => Run(() =>
    {
        if (GameProcess.IsRunning(Paths!))
        {
            return Loc.Get("GameAlreadyRunning");
        }
        SaveSettings();
        GameProcess.Launch(Paths!, _steamCommand);
        if (LaunchedBySteam)
        {
            WaitForGameThenExit();
        }
        return Loc.Get("GameLaunched");
    });
}
