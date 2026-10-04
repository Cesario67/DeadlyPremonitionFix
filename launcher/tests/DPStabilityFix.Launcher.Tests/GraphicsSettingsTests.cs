using DPStabilityFix.Launcher.Core;
using Xunit;

namespace DPStabilityFix.Launcher.Tests;

public sealed class GraphicsSettingsTests
{
    [Theory]
    [InlineData(AntiAliasing.Off, 1920, 1080, 0)]
    [InlineData(AntiAliasing.Smaa, 1920, 1080, 2)]
    [InlineData(AntiAliasing.SuperSampling2x, 2880, 1620, 2)]
    [InlineData(AntiAliasing.SuperSampling4x, 3840, 2160, 2)]
    [InlineData(AntiAliasing.SuperSampling9x, 5760, 3240, 2)]
    public void AntiAliasingWritesRenderResolutionAndSmaa(AntiAliasing level, int width, int height, int aaQuality)
    {
        DpfixConfig config = DpfixConfig.Parse(string.Empty);
        GraphicsSettingsMapper.Write(new GraphicsSettings { AntiAliasing = level }, config);
        Assert.Equal(width, config.GetInt("renderWidth", 0));
        Assert.Equal(height, config.GetInt("renderHeight", 0));
        Assert.Equal(aaQuality, config.GetInt("aaQuality", -1));
        Assert.Equal(1920, config.GetInt("presentWidth", 0));
    }

    [Theory]
    [InlineData(DisplayMode.Borderless, true, false)]
    [InlineData(DisplayMode.Fullscreen, false, false)]
    [InlineData(DisplayMode.Windowed, false, true)]
    public void DisplayModeMapsToDpfixFlags(DisplayMode mode, bool borderless, bool windowed)
    {
        DpfixConfig config = DpfixConfig.Parse(string.Empty);
        GraphicsSettingsMapper.Write(new GraphicsSettings { DisplayMode = mode }, config);
        Assert.Equal(borderless, config.GetBool("borderlessFullscreen", !borderless));
        Assert.Equal(windowed, config.GetBool("forceWindowed", !windowed));
        Assert.Equal(mode, GraphicsSettingsMapper.Read(config).DisplayMode);
    }

    [Fact]
    public void RoundTripKeepsEverySetting()
    {
        GraphicsSettings original = new()
        {
            DisplayMode = DisplayMode.Windowed,
            DisplayWidth = 2560,
            DisplayHeight = 1440,
            AntiAliasing = AntiAliasing.SuperSampling2x,
            AmbientOcclusion = EffectLevel.High,
            Shadows = DetailScale.VeryHigh,
            PreciseShadows = true,
            Reflections = DetailScale.High,
            ImprovedDepthOfField = true,
            AnisotropicFiltering = true,
        };
        DpfixConfig config = DpfixConfig.Parse(string.Empty);
        GraphicsSettingsMapper.Write(original, config);
        Assert.Equal(original, GraphicsSettingsMapper.Read(config));
    }

    [Fact]
    public void HandTunedRenderResolutionIsDetectedAsCustomAndKept()
    {
        DpfixConfig config = DpfixConfig.Parse("renderWidth 2500\nrenderHeight 1400\npresentWidth 1920\npresentHeight 1080\naaQuality 3");
        GraphicsSettings settings = GraphicsSettingsMapper.Read(config);
        Assert.Equal(AntiAliasing.Custom, settings.AntiAliasing);
        GraphicsSettingsMapper.Write(settings, config);
        Assert.Equal(2500, config.GetInt("renderWidth", 0));
        Assert.Equal(3, config.GetInt("aaQuality", 0));
    }

    [Fact]
    public void DepthOfFieldBlurFollowsInternalResolution()
    {
        DpfixConfig config = DpfixConfig.Parse(string.Empty);
        GraphicsSettingsMapper.Write(new GraphicsSettings { ImprovedDepthOfField = true, AntiAliasing = AntiAliasing.SuperSampling4x }, config);
        Assert.Equal(2, config.GetInt("addDOFBlur", 0));
        GraphicsSettingsMapper.Write(new GraphicsSettings { ImprovedDepthOfField = true, AntiAliasing = AntiAliasing.Smaa }, config);
        Assert.Equal(1, config.GetInt("addDOFBlur", 0));
    }
}

public sealed class StabilitySettingsTests
{
    [Fact]
    public void DefaultsMatchTheDllWhenIniIsEmpty()
    {
        StabilitySettings settings = StabilitySettings.Read(IniDocument.Parse(string.Empty));
        Assert.Equal(new StabilitySettings(), settings);
        Assert.True(settings.PreciseGameTime);
        Assert.True(settings.PreciseTimer);
    }

    [Fact]
    public void WritesDllKeys()
    {
        IniDocument ini = IniDocument.Parse(string.Empty);
        new StabilitySettings { PreciseTimer = false, PreciseGameTime = false, SaveBackupCount = 5000 }.Write(ini);
        Assert.Equal("0", ini.Get("Frames", "TimerResolutionMs"));
        Assert.Equal("0", ini.Get("Frames", "ForceFpuPreserve"));
        Assert.Equal("500", ini.Get("Save", "BackupCount"));
    }
}
