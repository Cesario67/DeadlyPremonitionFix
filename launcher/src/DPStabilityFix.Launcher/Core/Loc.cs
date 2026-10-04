using System.ComponentModel;
using System.Globalization;

namespace DPStabilityFix.Launcher.Core;

public enum Language
{
    French,
    English,
}

/// <summary>
/// Textes du launcher en français et en anglais. Les vues les lient par clé (<c>{Binding L[Save]}</c>, où
/// <c>L</c> est cette instance), le code les obtient par <see cref="Get(string)"/>. Le changement de langue
/// prévient les liaisons (indexeur) : tout le texte se met à jour sans redémarrer.
/// Pas de fichiers de ressources ni de réflexion : la table est dans <see cref="Translations"/>, compatible
/// avec l'élagage du .exe publié. La langue choisie est mémorisée dans %APPDATA%\DPStabilityFix\language.txt.
/// </summary>
public sealed class Loc : INotifyPropertyChanged
{
    private static readonly CultureInfo FrenchCulture = CultureInfo.GetCultureInfo("fr-FR");
    private static readonly CultureInfo EnglishCulture = CultureInfo.GetCultureInfo("en-US");

    private Language _language = DetectLanguage();

    private Loc()
    {
    }

    public static Loc Instance { get; } = new();

    public static Language Current => Instance._language;

    /// <summary>Culture de la langue choisie : séparateurs de milliers, etc.</summary>
    public static CultureInfo Culture => Current == Language.French ? FrenchCulture : EnglishCulture;

    /// <summary>Format de date des copies de secours, selon la langue.</summary>
    public static string DateTimeFormat => Current == Language.French ? "dd/MM/yyyy HH:mm:ss" : "yyyy-MM-dd HH:mm:ss";

    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>Levé après chaque changement de langue (pour les textes qui ne passent pas par une liaison).</summary>
    public event Action? LanguageChanged;

    private static string SettingsFile => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "DPStabilityFix", "language.txt");

    /// <summary>Texte d'une clé dans la langue courante ; « !clé! » si elle n'existe pas.</summary>
    public string this[string key] => Get(key);

    public static string Get(string key) =>
        Translations.Table.TryGetValue(key, out (string Fr, string En) texts)
            ? (Current == Language.French ? texts.Fr : texts.En)
            : $"!{key}!";

    public static string Get(string key, params object[] arguments) => string.Format(Culture, Get(key), arguments);

    /// <summary>Change de langue. <paramref name="remember"/> : mémoriser le choix pour les prochains lancements.</summary>
    public static void SetLanguage(Language language, bool remember = true)
    {
        Loc loc = Instance;
        if (loc._language == language)
        {
            return;
        }
        loc._language = language;
        if (remember)
        {
            Remember(language);
        }
        // « Item[] » : notification des indexeurs ; chaîne vide : toutes les propriétés.
        loc.PropertyChanged?.Invoke(loc, new PropertyChangedEventArgs("Item[]"));
        loc.PropertyChanged?.Invoke(loc, new PropertyChangedEventArgs(string.Empty));
        loc.LanguageChanged?.Invoke();
    }

    private static Language DetectLanguage()
    {
        try
        {
            if (File.Exists(SettingsFile))
            {
                string saved = File.ReadAllText(SettingsFile).Trim();
                if (string.Equals(saved, "fr", StringComparison.OrdinalIgnoreCase))
                {
                    return Language.French;
                }
                if (string.Equals(saved, "en", StringComparison.OrdinalIgnoreCase))
                {
                    return Language.English;
                }
            }
        }
        catch (IOException)
        {
            // Fichier illisible : on retombe sur la langue de Windows.
        }
        catch (UnauthorizedAccessException)
        {
        }
        return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "fr" ? Language.French : Language.English;
    }

    private static void Remember(Language language)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingsFile)!);
            File.WriteAllText(SettingsFile, language == Language.French ? "fr" : "en");
        }
        catch (IOException)
        {
            // Pas bloquant : la langue ne sera simplement pas mémorisée.
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
