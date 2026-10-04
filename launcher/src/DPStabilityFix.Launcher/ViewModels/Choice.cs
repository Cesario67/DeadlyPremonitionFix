namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Élément de liste déroulante : une valeur et son libellé affiché.</summary>
public sealed record Choice<T>(T Value, string Label)
{
    public override string ToString() => Label;
}
