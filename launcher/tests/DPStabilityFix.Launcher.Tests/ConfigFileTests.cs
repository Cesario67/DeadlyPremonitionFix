using DPStabilityFix.Launcher.Core;
using Xunit;

namespace DPStabilityFix.Launcher.Tests;

public sealed class IniDocumentTests
{
    private const string Sample = "; DPStabilityFix\n[Save]\n; commentaire\nAtomicWrites=1\nBackupCount=20\n\n[Frames]\nFrameStats=1\n";

    [Fact]
    public void ReadsValuesIgnoringCaseAndComments()
    {
        IniDocument ini = IniDocument.Parse(Sample);
        Assert.Equal("20", ini.Get("save", "backupcount"));
        Assert.True(ini.GetBool("Save", "AtomicWrites", false));
        Assert.Null(ini.Get("Save", "commentaire"));
        Assert.Equal(7, ini.GetInt("Save", "Absent", 7));
    }

    [Fact]
    public void UpdatesInPlaceAndKeepsComments()
    {
        IniDocument ini = IniDocument.Parse(Sample);
        ini.SetInt("Save", "BackupCount", 50);
        string text = ini.ToString();
        Assert.Contains("BackupCount=50", text);
        Assert.Contains("; commentaire", text);
        Assert.DoesNotContain("BackupCount=20", text);
    }

    [Fact]
    public void AddsMissingKeyInsideItsSection()
    {
        IniDocument ini = IniDocument.Parse(Sample);
        ini.SetBool("Save", "SaveGuard", true);
        string[] lines = ini.ToString().Split('\n');
        int saveGuard = Array.IndexOf(lines, "SaveGuard=1");
        Assert.True(saveGuard > Array.IndexOf(lines, "BackupCount=20"));
        Assert.True(saveGuard < Array.IndexOf(lines, "[Frames]"));
    }

    [Fact]
    public void AddsMissingSection()
    {
        IniDocument ini = IniDocument.Parse(Sample);
        ini.SetBool("Graphics", "IntegratedDPfix", false);
        Assert.False(ini.GetBool("Graphics", "IntegratedDPfix", true));
        Assert.EndsWith("[Graphics]\nIntegratedDPfix=0", ini.ToString().TrimEnd('\n'));
    }
}

public sealed class DpfixConfigTests
{
    private const string Sample = "# internal rendering resolution\nrenderWidth 1920\nrenderHeight 1080\n# AA\naaQuality 0\nssaoStrength 0\nssaoScale 1\n";

    [Fact]
    public void ReadsKeysExactlyNotByPrefix()
    {
        DpfixConfig config = DpfixConfig.Parse(Sample);
        Assert.Equal(1920, config.GetInt("renderWidth", 0));
        // ssaoScale ne doit pas être confondu avec ssaoStrength (DPfix lit par préfixe, nous non).
        Assert.Equal(1, config.GetInt("ssaoScale", 0));
        Assert.Equal(0, config.GetInt("ssaoStrength", 9));
    }

    [Fact]
    public void UpdatesInPlaceKeepsCommentsAndAppendsMissing()
    {
        DpfixConfig config = DpfixConfig.Parse(Sample);
        config.SetInt("renderWidth", 3840);
        config.SetBool("borderlessFullscreen", true);
        string text = config.ToString();
        Assert.Contains("renderWidth 3840", text);
        Assert.Contains("# internal rendering resolution", text);
        Assert.EndsWith("borderlessFullscreen 1", text.TrimEnd('\n'));
    }
}
