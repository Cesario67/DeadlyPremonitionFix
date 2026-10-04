using DPStabilityFix.Launcher.Core;
using Xunit;

namespace DPStabilityFix.Launcher.Tests;

public sealed class ExePatcherTests
{
    [Fact]
    public void EnablesLargeAddressAwareOnceAndKeepsOriginal()
    {
        using TestGame game = new();
        Assert.False(ExePatcher.ReadInfo(game.Paths.GameExe).LargeAddressAware);

        Assert.True(ExePatcher.EnableLargeAddressAware(game.Paths.GameExe));
        Assert.True(ExePatcher.ReadInfo(game.Paths.GameExe).LargeAddressAware);
        Assert.False(ExePatcher.ReadInfo(game.Paths.OriginalExeBackup).LargeAddressAware);

        byte[] original = File.ReadAllBytes(game.Paths.OriginalExeBackup);
        Assert.False(ExePatcher.EnableLargeAddressAware(game.Paths.GameExe));
        Assert.Equal(original, File.ReadAllBytes(game.Paths.OriginalExeBackup));
        Assert.False(File.Exists(game.Paths.GameExe + ".dpsf-tmp"));
    }

    [Fact]
    public void RestoresOriginal()
    {
        using TestGame game = new();
        ExePatcher.EnableLargeAddressAware(game.Paths.GameExe);
        ExePatcher.RestoreOriginal(game.Paths.GameExe);
        Assert.False(ExePatcher.ReadInfo(game.Paths.GameExe).LargeAddressAware);
    }

    [Fact]
    public void ReadsVersionAndRejectsInvalidFiles()
    {
        using TestGame game = new(timestamp: 0x12345678);
        ExeInfo info = ExePatcher.ReadInfo(game.Paths.GameExe);
        Assert.True(info.IsValid && info.IsX86);
        Assert.False(info.IsKnownVersion);

        File.WriteAllText(game.Paths.GameExe, "pas un exécutable");
        Assert.False(ExePatcher.ReadInfo(game.Paths.GameExe).IsValid);
    }
}

[Collection("Language")]
public sealed class SaveBackupsTests
{
    [Fact]
    public void ListsBackupsNewestFirstAndRestoresSafely()
    {
        using TestGame game = new();
        Directory.CreateDirectory(game.Paths.SaveBackupsDirectory);
        Directory.CreateDirectory(Path.GetDirectoryName(game.Paths.SaveFile)!);
        File.WriteAllText(game.Paths.SaveFile, "actuelle");
        File.WriteAllText(Path.Combine(game.Paths.SaveBackupsDirectory, "dp_20261003-100000-000_demarrage.sav"), "ancienne");
        File.WriteAllText(Path.Combine(game.Paths.SaveBackupsDirectory, "recovered_20261003-120000-000_ecriture-interrompue.sav"), "incomplète");
        File.WriteAllText(Path.Combine(game.Paths.SaveBackupsDirectory, "autre-fichier.sav"), "ignoré");

        IReadOnlyList<SaveBackup> backups = SaveBackups.List(game.Paths);
        Assert.Equal(2, backups.Count);
        Assert.True(backups[0].IsIncomplete);
        // Texte de la langue courante (français sur le PC du développeur, anglais sur le runner de la CI).
        Assert.Equal(Loc.Get("BackupStart"), backups[1].Description);

        SaveBackups.Restore(game.Paths, backups[1]);
        Assert.Equal("ancienne", File.ReadAllText(game.Paths.SaveFile));
        SaveBackup setAside = Assert.Single(SaveBackups.List(game.Paths), backup => backup.Reason == "avant-restauration");
        Assert.Equal("actuelle", File.ReadAllText(setAside.Path));
    }
}

public sealed class ModInstallerTests
{
    [Fact]
    public void RejectsPackageWithoutModDll()
    {
        using TestGame game = new();
        string package = Path.Combine(game.Root, "paquet-vide");
        Directory.CreateDirectory(package);
        Assert.Throws<FileNotFoundException>(() => ModInstaller.Install(package, game.Paths, null));
        Assert.False(ModInstaller.IsPackageDirectory(package));
    }

    [Fact]
    public void RejectsInvalidGameExecutable()
    {
        using TestGame game = new();
        File.WriteAllText(game.Paths.GameExe, "pas un exécutable");
        Assert.Throws<InvalidDataException>(() => ModInstaller.Install(game.Root, game.Paths, null));
    }

    [Fact]
    public void DisablesAndRestoresExternalDpfixWithoutDeletingIt()
    {
        using TestGame game = new();
        File.WriteAllText(game.Paths.ExternalDpfixDll, "DPfix d'origine");
        ModInstaller.DisableExternalDpfix(game.Paths);
        Assert.False(File.Exists(game.Paths.ExternalDpfixDll));
        Assert.Equal("DPfix d'origine", File.ReadAllText(game.Paths.DisabledExternalDpfixDll));
        ModInstaller.RestoreExternalDpfix(game.Paths);
        Assert.True(File.Exists(game.Paths.ExternalDpfixDll));
    }
}
