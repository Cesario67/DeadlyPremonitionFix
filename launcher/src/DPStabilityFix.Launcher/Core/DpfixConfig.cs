using System.Text;

namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// DPfix.ini : une ligne « clé valeur » par réglage, commentaires commençant par « # ». Modifié en place
/// (commentaires et ordre conservés) ; une clé absente est ajoutée à la fin.
/// </summary>
/// <remarks>
/// DPfix lit le fichier par lignes de 127 caractères au plus et reconnaît une clé par préfixe : les
/// valeurs écrites ici restent courtes.
/// </remarks>
public sealed class DpfixConfig
{
    private readonly List<string> _lines;

    private DpfixConfig(List<string> lines) => _lines = lines;

    public static DpfixConfig Load(string path) =>
        new(File.Exists(path) ? File.ReadAllLines(path, Encoding.UTF8).ToList() : new List<string>());

    public static DpfixConfig Parse(string content) =>
        new(content.Replace("\r\n", "\n").Split('\n').ToList());

    public string? Get(string key)
    {
        int index = FindKey(key);
        return index < 0 ? null : _lines[index].Trim()[key.Length..].Trim();
    }

    public int GetInt(string key, int fallback) =>
        Get(key) is { } value && int.TryParse(value, out int number) ? number : fallback;

    public bool GetBool(string key, bool fallback) => Get(key) is { } value ? value is "1" or "true" : fallback;

    public void Set(string key, string value)
    {
        string line = $"{key} {value}";
        int index = FindKey(key);
        if (index >= 0)
        {
            _lines[index] = line;
        }
        else
        {
            _lines.Add(line);
        }
    }

    public void SetInt(string key, int value) => Set(key, value.ToString());

    public void SetBool(string key, bool value) => Set(key, value ? "1" : "0");

    // CRLF : le format d'origine du fichier livré par Durante.
    public void Save(string path) => File.WriteAllText(path, string.Join("\r\n", _lines) + "\r\n", new UTF8Encoding(false));

    public override string ToString() => string.Join("\n", _lines);

    private int FindKey(string key)
    {
        for (int i = 0; i < _lines.Count; i++)
        {
            string line = _lines[i].Trim();
            if (line.StartsWith('#'))
            {
                continue;
            }
            int space = line.IndexOfAny([' ', '\t']);
            string name = space < 0 ? line : line[..space];
            if (string.Equals(name, key, StringComparison.Ordinal))
            {
                return i;
            }
        }
        return -1;
    }
}
