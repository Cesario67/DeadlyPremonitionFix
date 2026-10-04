using DPStabilityFix.Launcher.Core;
using Xunit;

namespace DPStabilityFix.Launcher.Tests;

public sealed class DefaultSettingsTests
{
    private const string ModifiedDpfix =
        "# Réglages de DPfix\nrenderWidth 3840\nrenderHeight 2160\npresentWidth 2560\npresentHeight 1440\n" +
        "aaQuality 2\naaType SMAA\nssaoStrength 3\nshadowMapScale 4\nimproveDOF 1\naddDOFBlur 2\n" +
        "filteringOverride 2\nborderlessFullscreen 1\nlogLevel 2\nscreenshotDir dpfix\\screens\n";

    private const string ModifiedMod =
        "; Réglages du mod\n[Frames]\nFrameLimitFps=0\nForceFpuPreserve=0\n[Controller]\nSwapTriggers=1\n" +
        "BackgroundPolling=0\n[Gameplay]\nSkipIntro=0\n";

    [Fact]
    public void RemovesAntiAliasingAndEffectsButKeepsTheDisplay()
    {
        DpfixConfig dpfix = DpfixConfig.Parse(ModifiedDpfix);
        DefaultSettings.Apply(dpfix, IniDocument.Parse(ModifiedMod));

        GraphicsSettings settings = GraphicsSettingsMapper.Read(dpfix);
        Assert.Equal(AntiAliasing.Off, settings.AntiAliasing);
        Assert.Equal(0, dpfix.GetInt("aaQuality", -1));
        Assert.Equal(2560, dpfix.GetInt("renderWidth", 0));
        Assert.Equal(EffectLevel.Off, settings.AmbientOcclusion);
        Assert.Equal(DetailScale.Default, settings.Shadows);
        Assert.False(settings.ImprovedDepthOfField);
        Assert.False(settings.AnisotropicFiltering);
        Assert.Equal(0, dpfix.GetInt("logLevel", -1));
        // Affichage du joueur et clés inconnues du launcher conservés.
        Assert.Equal(DisplayMode.Borderless, settings.DisplayMode);
        Assert.Equal((2560, 1440), (settings.DisplayWidth, settings.DisplayHeight));
        Assert.Equal("dpfix\\screens", dpfix.Get("screenshotDir"));
    }

    [Fact]
    public void RestoresModOptionsAndKeepsComments()
    {
        IniDocument mod = IniDocument.Parse(ModifiedMod);
        DefaultSettings.Apply(DpfixConfig.Parse(ModifiedDpfix), mod);

        Assert.Equal(new StabilitySettings(), StabilitySettings.Read(mod));
        Assert.Equal("60", mod.Get("Frames", "FrameLimitFps"));
        Assert.Equal("1", mod.Get("Frames", "ForceFpuPreserve"));
        Assert.Equal("0", mod.Get("Controller", "SwapTriggers"));
        Assert.Equal("1", mod.Get("Gameplay", "SkipIntro"));
        Assert.Contains("; Réglages du mod", mod.ToString());
    }
}
