using System.ComponentModel;
using System.Text.RegularExpressions;
using DPStabilityFix.Launcher.Core;
using Xunit;

namespace DPStabilityFix.Launcher.Tests;

// La langue du launcher (Loc) est un état global : les classes qui la changent ou lisent des textes traduits
// partagent cette collection, donc ne s'exécutent jamais en même temps.
[Collection("Language")]
public sealed partial class LocalizationTests
{
    [GeneratedRegex(@"\{(\d+)(?::[^}]*)?\}")]
    private static partial Regex Placeholder();

    // L[Key] dans le XAML ; Loc.Get("Key"...) et Choice<...>.Localized(..., "Key") dans le code.
    [GeneratedRegex(@"\bL\[(\w+)\]")]
    private static partial Regex XamlKey();

    [GeneratedRegex(@"Loc\.Get\(""(\w+)""")]
    private static partial Regex CodeKey();

    [GeneratedRegex(@"\.Localized\([^,]+,\s*""(\w+)""")]
    private static partial Regex ChoiceKey();

    private static string LauncherSourceDirectory()
    {
        DirectoryInfo? directory = new(AppContext.BaseDirectory);
        while (directory is not null && !Directory.Exists(Path.Combine(directory.FullName, "src", "DPStabilityFix.Launcher")))
        {
            directory = directory.Parent;
        }
        return Path.Combine(directory?.FullName ?? throw new DirectoryNotFoundException("launcher/src"), "src", "DPStabilityFix.Launcher");
    }

    private static HashSet<string> KeysUsedInSources()
    {
        string source = LauncherSourceDirectory();
        HashSet<string> keys = [];
        foreach (string file in Directory.EnumerateFiles(source, "*.axaml", SearchOption.AllDirectories))
        {
            foreach (Match match in XamlKey().Matches(File.ReadAllText(file)))
            {
                keys.Add(match.Groups[1].Value);
            }
        }
        foreach (string file in Directory.EnumerateFiles(source, "*.cs", SearchOption.AllDirectories)
                                         .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                                                        !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}")))
        {
            string text = File.ReadAllText(file);
            foreach (Match match in CodeKey().Matches(text).Concat(ChoiceKey().Matches(text)))
            {
                keys.Add(match.Groups[1].Value);
            }
        }
        return keys;
    }

    [Fact]
    public void EveryKeyUsedInViewsAndCodeExists()
    {
        HashSet<string> used = KeysUsedInSources();
        Assert.True(used.Count > 100, "Les clés devraient être trouvées dans les sources.");
        string[] missing = used.Where(key => !Translations.Table.ContainsKey(key)).Order().ToArray();
        Assert.True(missing.Length == 0, "Clés sans traduction : " + string.Join(", ", missing));
    }

    [Fact]
    public void NoUnusedKeys()
    {
        HashSet<string> used = KeysUsedInSources();
        string[] unused = Translations.Table.Keys.Where(key => !used.Contains(key)).Order().ToArray();
        Assert.True(unused.Length == 0, "Clés jamais employées : " + string.Join(", ", unused));
    }

    [Fact]
    public void BothLanguagesAreFilledWithTheSameArguments()
    {
        foreach ((string key, (string fr, string en)) in Translations.Table)
        {
            Assert.False(string.IsNullOrWhiteSpace(fr), $"{key} : français vide");
            Assert.False(string.IsNullOrWhiteSpace(en), $"{key} : anglais vide");
            string[] frenchArguments = Placeholder().Matches(fr).Select(match => match.Groups[1].Value).Order().ToArray();
            string[] englishArguments = Placeholder().Matches(en).Select(match => match.Groups[1].Value).Order().ToArray();
            Assert.True(frenchArguments.SequenceEqual(englishArguments), $"{key} : arguments différents entre les langues");
        }
    }

    [Fact]
    public void NoEmDashInTexts()
    {
        foreach ((string key, (string fr, string en)) in Translations.Table)
        {
            Assert.DoesNotContain('—', fr + en);
        }
    }

    [Fact]
    public void SwitchingLanguageChangesTextsAndNotifiesBindings()
    {
        Language original = Loc.Current;
        List<string?> notifications = [];
        PropertyChangedEventHandler handler = (_, args) => notifications.Add(args.PropertyName);
        Loc.Instance.PropertyChanged += handler;
        try
        {
            Loc.SetLanguage(Language.French, remember: false);
            Assert.Equal("Enregistrer", Loc.Instance["Save"]);
            notifications.Clear();

            Loc.SetLanguage(Language.English, remember: false);
            Assert.Equal("Save", Loc.Instance["Save"]);
            Assert.Equal("Item[]", notifications.First());
            Assert.Equal("Image rendered at 3840 × 2160, displayed at 1920 × 1080.", Loc.Get("RenderResolution", 3840, 2160, 1920, 1080));
            Assert.Equal("!UnknownKey!", Loc.Get("UnknownKey"));

            Loc.SetLanguage(Language.French, remember: false);
            Assert.Equal("Image calculée en 3840 × 2160, affichée en 1920 × 1080.", Loc.Get("RenderResolution", 3840, 2160, 1920, 1080));
        }
        finally
        {
            Loc.Instance.PropertyChanged -= handler;
            Loc.SetLanguage(original, remember: false);
        }
    }

    [Fact]
    public void BackupDescriptionsFollowTheLanguage()
    {
        Language original = Loc.Current;
        try
        {
            SaveBackup backup = new("x.sav", DateTime.Now, 10, "avant-ecriture");
            Loc.SetLanguage(Language.French, remember: false);
            Assert.Equal("Avant une sauvegarde", backup.Description);
            Loc.SetLanguage(Language.English, remember: false);
            Assert.Equal("Before a save", backup.Description);
        }
        finally
        {
            Loc.SetLanguage(original, remember: false);
        }
    }
}
