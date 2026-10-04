using CommunityToolkit.Mvvm.ComponentModel;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>
/// Base des ViewModels dont la vue contient du texte traduit : expose <see cref="L"/>, que le XAML lie par clé
/// (<c>{Binding L[Save]}</c>) et qui prévient du changement de langue.
/// </summary>
public abstract class LocalizedViewModel : ObservableObject
{
    public Loc L => Loc.Instance;
}
