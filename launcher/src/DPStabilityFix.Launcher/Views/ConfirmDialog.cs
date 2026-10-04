using Avalonia.Controls;
using Avalonia.Layout;

namespace DPStabilityFix.Launcher.Views;

/// <summary>Petite fenêtre « Oui / Non » (Avalonia n'a pas de boîte de message intégrée).</summary>
public static class ConfirmDialog
{
    public static Task<bool> ShowAsync(Window owner, string message)
    {
        Window dialog = new()
        {
            Title = "DPStabilityFix",
            Width = 460,
            SizeToContent = SizeToContent.Height,
            CanResize = false,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
        };
        Button yes = new() { Content = "Oui", MinWidth = 90, HorizontalContentAlignment = HorizontalAlignment.Center };
        Button no = new() { Content = "Non", MinWidth = 90, HorizontalContentAlignment = HorizontalAlignment.Center };
        yes.Classes.Add("accent");
        yes.Click += (_, _) => dialog.Close(true);
        no.Click += (_, _) => dialog.Close(false);
        dialog.Content = new StackPanel
        {
            Margin = new Avalonia.Thickness(20),
            Spacing = 16,
            Children =
            {
                new TextBlock { Text = message, TextWrapping = Avalonia.Media.TextWrapping.Wrap },
                new StackPanel
                {
                    Orientation = Orientation.Horizontal,
                    HorizontalAlignment = HorizontalAlignment.Right,
                    Spacing = 8,
                    Children = { yes, no },
                },
            },
        };
        return dialog.ShowDialog<bool>(owner);
    }
}
