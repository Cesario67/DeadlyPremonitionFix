using System.Reflection;

namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// Le launcher peut contenir tout ce qu'il installe (DLL du mod, .ini, shaders de DPfix, licence) : les
/// joueurs ne téléchargent alors que <c>DPStabilityFix.exe</c>. Ces fichiers sont des ressources intégrées de
/// nom <c>payload/...</c> (voir le .csproj) ; au démarrage ils sont décompressés dans un dossier de l'utilisateur,
/// qui joue le rôle du « paquet » pour <see cref="ModInstaller"/>.
/// </summary>
public static class EmbeddedPackage
{
    public const string ResourcePrefix = "payload/";

    private const string StampFile = "payload.stamp";

    private static string DefaultTargetDirectory => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DPStabilityFix", "package");

    /// <summary>
    /// Dossier du paquet à installer : celui décompressé depuis le .exe s'il en contient un, sinon le dossier
    /// du launcher (paquet complet décompressé à la main, ou compilation de développement sans DLL intégrée).
    /// </summary>
    public static string Resolve(string launcherDirectory)
    {
        Assembly assembly = typeof(EmbeddedPackage).Assembly;
        string[] names = assembly.GetManifestResourceNames()
                                 .Where(name => name.StartsWith(ResourcePrefix, StringComparison.Ordinal))
                                 .ToArray();
        if (names.Length == 0)
        {
            return launcherDirectory;
        }
        string stamp = ComputeStamp();
        return Extract(names.Select(name => (name[ResourcePrefix.Length..], (Func<Stream>)(() => assembly.GetManifestResourceStream(name)!))),
                       DefaultTargetDirectory, stamp);
    }

    /// <summary>
    /// Écrit les fichiers dans <paramref name="targetDirectory"/> (noms relatifs, séparateur « / »). Si
    /// <paramref name="stamp"/> est celle de la dernière extraction, rien n'est réécrit. Renvoie le dossier.
    /// </summary>
    public static string Extract(IEnumerable<(string Name, Func<Stream> Open)> files, string targetDirectory, string stamp)
    {
        string stampPath = Path.Combine(targetDirectory, StampFile);
        List<(string Name, Func<Stream> Open)> list = files.ToList();
        bool upToDate = File.Exists(stampPath) && File.ReadAllText(stampPath) == stamp &&
                        list.All(file => File.Exists(Path.Combine(targetDirectory, file.Name.Replace('/', Path.DirectorySeparatorChar))));
        if (upToDate)
        {
            return targetDirectory;
        }
        foreach ((string name, Func<Stream> open) in list)
        {
            string path = Path.Combine(targetDirectory, name.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            using Stream source = open();
            using FileStream target = File.Create(path);
            source.CopyTo(target);
        }
        File.WriteAllText(stampPath, stamp);
        return targetDirectory;
    }

    // Identifie la version du .exe : taille et date de la dernière modification.
    private static string ComputeStamp()
    {
        if (Environment.ProcessPath is { } path && File.Exists(path))
        {
            FileInfo info = new(path);
            return $"{info.Length}-{info.LastWriteTimeUtc.Ticks}";
        }
        return "?";
    }
}
