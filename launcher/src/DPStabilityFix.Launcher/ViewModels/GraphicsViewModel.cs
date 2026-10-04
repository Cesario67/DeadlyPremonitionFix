using CommunityToolkit.Mvvm.ComponentModel;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Onglet Graphismes : réglages de DPfix intégré (DPfix.ini).</summary>
public sealed partial class GraphicsViewModel : LocalizedViewModel
{
    private int _customRenderWidth;
    private int _customRenderHeight;

    public IReadOnlyList<Choice<DisplayMode>> DisplayModes { get; } =
    [
        Choice<DisplayMode>.Localized(DisplayMode.Borderless, "ModeBorderless"),
        Choice<DisplayMode>.Localized(DisplayMode.Fullscreen, "ModeFullscreen"),
        Choice<DisplayMode>.Localized(DisplayMode.Windowed, "ModeWindowed"),
    ];

    public IReadOnlyList<Choice<AntiAliasing>> AntiAliasingLevels { get; } =
    [
        Choice<AntiAliasing>.Localized(AntiAliasing.Off, "AaOff"),
        Choice<AntiAliasing>.Localized(AntiAliasing.Smaa, "AaSmaa"),
        Choice<AntiAliasing>.Localized(AntiAliasing.SuperSampling2x, "AaSuper2"),
        Choice<AntiAliasing>.Localized(AntiAliasing.SuperSampling4x, "AaSuper4"),
        Choice<AntiAliasing>.Localized(AntiAliasing.SuperSampling9x, "AaSuper9"),
        Choice<AntiAliasing>.Localized(AntiAliasing.Custom, "AaCustom"),
    ];

    public IReadOnlyList<Choice<EffectLevel>> AmbientOcclusionLevels { get; } =
    [
        Choice<EffectLevel>.Localized(EffectLevel.Off, "SsaoOff"),
        Choice<EffectLevel>.Localized(EffectLevel.Low, "SsaoLow"),
        Choice<EffectLevel>.Localized(EffectLevel.Medium, "SsaoMedium"),
        Choice<EffectLevel>.Localized(EffectLevel.High, "SsaoHigh"),
    ];

    public IReadOnlyList<Choice<DetailScale>> DetailScales { get; } =
    [
        Choice<DetailScale>.Localized(DetailScale.Default, "ScaleDefault"),
        Choice<DetailScale>.Localized(DetailScale.High, "ScaleHigh"),
        Choice<DetailScale>.Localized(DetailScale.VeryHigh, "ScaleVeryHigh"),
    ];

    public IReadOnlyList<Choice<(int Width, int Height)>> Resolutions { get; private set; } = BuildResolutions(1920, 1080);

    [ObservableProperty]
    public partial Choice<DisplayMode> SelectedDisplayMode { get; set; }

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(RenderResolutionText))]
    public partial Choice<(int Width, int Height)> SelectedResolution { get; set; }

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(RenderResolutionText))]
    public partial Choice<AntiAliasing> SelectedAntiAliasing { get; set; }

    [ObservableProperty]
    public partial Choice<EffectLevel> SelectedAmbientOcclusion { get; set; }

    [ObservableProperty]
    public partial Choice<DetailScale> SelectedShadows { get; set; }

    [ObservableProperty]
    public partial bool PreciseShadows { get; set; }

    [ObservableProperty]
    public partial Choice<DetailScale> SelectedReflections { get; set; }

    [ObservableProperty]
    public partial bool ImprovedDepthOfField { get; set; }

    [ObservableProperty]
    public partial bool AnisotropicFiltering { get; set; }

    public GraphicsViewModel()
    {
        SelectedDisplayMode = DisplayModes[0];
        SelectedResolution = Resolutions.First(choice => choice.Value == (1920, 1080));
        SelectedAntiAliasing = AntiAliasingLevels[3];
        SelectedAmbientOcclusion = AmbientOcclusionLevels[0];
        SelectedShadows = DetailScales[0];
        SelectedReflections = DetailScales[0];
        Loc.Instance.LanguageChanged += RefreshLabels;
    }

    /// <summary>Après un changement de langue : libellés des listes et phrase de résolution relus.</summary>
    private void RefreshLabels()
    {
        IEnumerable<ChoiceBase> choices = [.. DisplayModes, .. AntiAliasingLevels, .. AmbientOcclusionLevels, .. DetailScales];
        foreach (ChoiceBase choice in choices)
        {
            choice.RefreshLabel();
        }
        OnPropertyChanged(nameof(RenderResolutionText));
    }

    public string RenderResolutionText
    {
        get
        {
            (int width, int height) = ToSettings().RenderResolution(_customRenderWidth, _customRenderHeight);
            return Loc.Get("RenderResolution", width, height, SelectedResolution.Value.Width,
                SelectedResolution.Value.Height);
        }
    }

    public void Load(DpfixConfig config)
    {
        GraphicsSettings settings = GraphicsSettingsMapper.Read(config);
        _customRenderWidth = config.GetInt("renderWidth", settings.DisplayWidth);
        _customRenderHeight = config.GetInt("renderHeight", settings.DisplayHeight);
        Resolutions = BuildResolutions(settings.DisplayWidth, settings.DisplayHeight);
        OnPropertyChanged(nameof(Resolutions));

        SelectedDisplayMode = DisplayModes.First(choice => choice.Value == settings.DisplayMode);
        SelectedResolution = Resolutions.First(choice => choice.Value == (settings.DisplayWidth, settings.DisplayHeight));
        SelectedAntiAliasing = AntiAliasingLevels.First(choice => choice.Value == settings.AntiAliasing);
        SelectedAmbientOcclusion = AmbientOcclusionLevels.First(choice => choice.Value == settings.AmbientOcclusion);
        SelectedShadows = DetailScales.First(choice => choice.Value == settings.Shadows);
        PreciseShadows = settings.PreciseShadows;
        SelectedReflections = DetailScales.First(choice => choice.Value == settings.Reflections);
        ImprovedDepthOfField = settings.ImprovedDepthOfField;
        AnisotropicFiltering = settings.AnisotropicFiltering;
    }

    public void Save(DpfixConfig config) => GraphicsSettingsMapper.Write(ToSettings(), config);

    private GraphicsSettings ToSettings() => new()
    {
        DisplayMode = SelectedDisplayMode.Value,
        DisplayWidth = SelectedResolution.Value.Width,
        DisplayHeight = SelectedResolution.Value.Height,
        AntiAliasing = SelectedAntiAliasing.Value,
        AmbientOcclusion = SelectedAmbientOcclusion.Value,
        Shadows = SelectedShadows.Value,
        PreciseShadows = PreciseShadows,
        Reflections = SelectedReflections.Value,
        ImprovedDepthOfField = ImprovedDepthOfField,
        AnisotropicFiltering = AnisotropicFiltering,
    };

    private static IReadOnlyList<Choice<(int Width, int Height)>> BuildResolutions(int currentWidth, int currentHeight)
    {
        List<(int Width, int Height)> sizes =
            [(1280, 720), (1600, 900), (1920, 1080), (2560, 1440), (3440, 1440), (3840, 2160)];
        if (!sizes.Contains((currentWidth, currentHeight)))
        {
            sizes.Add((currentWidth, currentHeight));
        }
        return sizes.OrderBy(size => size.Width * size.Height)
                    .Select(size => new Choice<(int Width, int Height)>(size, $"{size.Width} × {size.Height}"))
                    .ToList();
    }
}
