using CommunityToolkit.Mvvm.ComponentModel;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Base non générique des éléments de liste déroulante (le modèle XAML lie <see cref="Label"/>).</summary>
public abstract class ChoiceBase : ObservableObject
{
    public abstract string Label { get; }

    /// <summary>À appeler après un changement de langue : le libellé est relu dans la nouvelle langue.</summary>
    public void RefreshLabel() => OnPropertyChanged(nameof(Label));

    public override string ToString() => Label;
}

/// <summary>
/// Élément de liste déroulante : une valeur et son libellé, fixe (« 1920 × 1080 ») ou traduit selon la langue
/// (clé de <see cref="Translations"/>).
/// </summary>
public sealed class Choice<T> : ChoiceBase
{
    private readonly string? _key;
    private readonly string _fixedLabel;

    /// <summary>Libellé fixe, identique dans toutes les langues.</summary>
    public Choice(T value, string label)
    {
        Value = value;
        _fixedLabel = label;
    }

    private Choice(T value, string key, bool localized)
    {
        Value = value;
        _key = localized ? key : null;
        _fixedLabel = localized ? string.Empty : key;
    }

    /// <summary>Libellé traduit : <paramref name="key"/> est une clé de la table des textes.</summary>
    public static Choice<T> Localized(T value, string key) => new(value, key, localized: true);

    public T Value { get; }

    public override string Label => _key is null ? _fixedLabel : Loc.Get(_key);
}
