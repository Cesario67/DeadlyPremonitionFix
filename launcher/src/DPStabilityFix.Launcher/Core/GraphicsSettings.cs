namespace DPStabilityFix.Launcher.Core;

public enum DisplayMode
{
    Borderless,
    Fullscreen,
    Windowed,
}

/// <summary>
/// Qualité d'anticrénelage proposée au joueur. Le suréchantillonnage calcule l'image à une résolution
/// interne plus élevée puis la réduit (renderWidth/renderHeight de DPfix), SMAA lisse les contours restants.
/// </summary>
public enum AntiAliasing
{
    Off,
    Smaa,
    SuperSampling2x,   // ×1,5 par axe, 2,25 fois les pixels
    SuperSampling4x,   // ×2 par axe
    SuperSampling9x,   // ×3 par axe
    Custom,            // résolution interne réglée à la main dans DPfix.ini
}

public enum EffectLevel
{
    Off,
    Low,
    Medium,
    High,
}

public enum DetailScale
{
    Default = 1,
    High = 2,
    VeryHigh = 4,
}

/// <summary>Réglages graphiques de DPfix intégré, tels que présentés dans le launcher.</summary>
public sealed record GraphicsSettings
{
    public DisplayMode DisplayMode { get; init; } = DisplayMode.Borderless;
    public int DisplayWidth { get; init; } = 1920;
    public int DisplayHeight { get; init; } = 1080;
    public AntiAliasing AntiAliasing { get; init; } = AntiAliasing.SuperSampling4x;
    public EffectLevel AmbientOcclusion { get; init; } = EffectLevel.Off;
    public DetailScale Shadows { get; init; } = DetailScale.Default;
    public bool PreciseShadows { get; init; }
    public DetailScale Reflections { get; init; } = DetailScale.Default;
    public bool ImprovedDepthOfField { get; init; }
    public bool AnisotropicFiltering { get; init; }

    /// <summary>Résolution interne réellement calculée (renderWidth × renderHeight).</summary>
    public (int Width, int Height) RenderResolution(int customWidth = 0, int customHeight = 0) =>
        AntiAliasing switch
        {
            AntiAliasing.SuperSampling2x => (DisplayWidth * 3 / 2, DisplayHeight * 3 / 2),
            AntiAliasing.SuperSampling4x => (DisplayWidth * 2, DisplayHeight * 2),
            AntiAliasing.SuperSampling9x => (DisplayWidth * 3, DisplayHeight * 3),
            AntiAliasing.Custom when customWidth > 0 && customHeight > 0 => (customWidth, customHeight),
            _ => (DisplayWidth, DisplayHeight),
        };
}

/// <summary>Conversion entre <see cref="GraphicsSettings"/> et les clés de DPfix.ini.</summary>
public static class GraphicsSettingsMapper
{
    private const int SmaaQuality = 2; // « medium » : bon compromis, l'essentiel du lissage vient du suréchantillonnage

    public static GraphicsSettings Read(DpfixConfig config)
    {
        int presentWidth = config.GetInt("presentWidth", 1920);
        int presentHeight = config.GetInt("presentHeight", 1080);
        int renderWidth = config.GetInt("renderWidth", presentWidth);
        int renderHeight = config.GetInt("renderHeight", presentHeight);
        bool smaa = config.GetInt("aaQuality", 0) > 0;

        DisplayMode mode = config.GetBool("borderlessFullscreen", false) ? DisplayMode.Borderless
            : config.GetBool("forceWindowed", false) ? DisplayMode.Windowed
            : DisplayMode.Fullscreen;

        return new GraphicsSettings
        {
            DisplayMode = mode,
            DisplayWidth = presentWidth,
            DisplayHeight = presentHeight,
            AntiAliasing = DetectAntiAliasing(presentWidth, presentHeight, renderWidth, renderHeight, smaa),
            AmbientOcclusion = (EffectLevel)Math.Clamp(config.GetInt("ssaoStrength", 0), 0, 3),
            Shadows = ToScale(config.GetInt("shadowMapScale", 1)),
            PreciseShadows = config.GetBool("improveShadowPrecision", false),
            Reflections = ToScale(config.GetInt("reflectionScale", 1)),
            ImprovedDepthOfField = config.GetBool("improveDOF", false),
            AnisotropicFiltering = config.GetInt("filteringOverride", 0) == 2,
        };
    }

    public static void Write(GraphicsSettings settings, DpfixConfig config)
    {
        config.SetBool("borderlessFullscreen", settings.DisplayMode == DisplayMode.Borderless);
        config.SetBool("forceWindowed", settings.DisplayMode == DisplayMode.Windowed);
        config.SetInt("presentWidth", settings.DisplayWidth);
        config.SetInt("presentHeight", settings.DisplayHeight);

        if (settings.AntiAliasing != AntiAliasing.Custom)
        {
            (int width, int height) = settings.RenderResolution();
            config.SetInt("renderWidth", width);
            config.SetInt("renderHeight", height);
            config.SetInt("aaQuality", settings.AntiAliasing == AntiAliasing.Off ? 0 : SmaaQuality);
            config.Set("aaType", "SMAA");
        }

        config.SetInt("ssaoStrength", (int)settings.AmbientOcclusion);
        config.SetInt("shadowMapScale", (int)settings.Shadows);
        config.SetBool("improveShadowPrecision", settings.PreciseShadows);
        config.SetInt("reflectionScale", (int)settings.Reflections);
        config.SetBool("improveDOF", settings.ImprovedDepthOfField);
        // Flou supplémentaire de DPfix, ajusté à la résolution interne quand la profondeur de champ est améliorée.
        int renderHeight = settings.RenderResolution(config.GetInt("renderWidth", 0), config.GetInt("renderHeight", 0)).Height;
        config.SetInt("addDOFBlur", !settings.ImprovedDepthOfField ? 0 : renderHeight >= 2000 ? 2 : 1);
        config.SetInt("filteringOverride", settings.AnisotropicFiltering ? 2 : 0);
    }

    private static AntiAliasing DetectAntiAliasing(int presentWidth, int presentHeight, int renderWidth,
                                                   int renderHeight, bool smaa)
    {
        if (renderWidth == presentWidth && renderHeight == presentHeight)
        {
            return smaa ? AntiAliasing.Smaa : AntiAliasing.Off;
        }
        if (renderWidth == presentWidth * 3 / 2 && renderHeight == presentHeight * 3 / 2)
        {
            return AntiAliasing.SuperSampling2x;
        }
        if (renderWidth == presentWidth * 2 && renderHeight == presentHeight * 2)
        {
            return AntiAliasing.SuperSampling4x;
        }
        if (renderWidth == presentWidth * 3 && renderHeight == presentHeight * 3)
        {
            return AntiAliasing.SuperSampling9x;
        }
        return AntiAliasing.Custom;
    }

    private static DetailScale ToScale(int value) => value switch
    {
        >= 4 => DetailScale.VeryHigh,
        >= 2 => DetailScale.High,
        _ => DetailScale.Default,
    };
}
