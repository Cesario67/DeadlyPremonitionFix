using System.Text;

namespace DPStabilityFix.Launcher.Core;

/// <summary>
/// Fichier INI Windows ([Section] puis Clé=Valeur), modifié en place : commentaires, ordre et lignes
/// inconnues sont conservés. Une clé absente est ajoutée à la fin de sa section (créée si besoin).
/// Utilisé pour DPStabilityFix.ini.
/// </summary>
public sealed class IniDocument
{
    private readonly List<string> _lines;

    private IniDocument(List<string> lines) => _lines = lines;

    public static IniDocument Load(string path) =>
        new(File.Exists(path) ? File.ReadAllLines(path, Encoding.UTF8).ToList() : new List<string>());

    public static IniDocument Parse(string content) =>
        new(content.Replace("\r\n", "\n").Split('\n').ToList());

    public string? Get(string section, string key)
    {
        int index = FindKey(section, key);
        return index < 0 ? null : ValueOf(_lines[index]);
    }

    public bool GetBool(string section, string key, bool fallback) =>
        Get(section, key) is { } value && int.TryParse(value, out int number) ? number != 0 : fallback;

    public int GetInt(string section, string key, int fallback) =>
        Get(section, key) is { } value && int.TryParse(value, out int number) ? number : fallback;

    public void Set(string section, string key, string value)
    {
        int index = FindKey(section, key);
        if (index >= 0)
        {
            _lines[index] = $"{key}={value}";
            return;
        }
        int sectionLine = FindSection(section);
        if (sectionLine < 0)
        {
            if (_lines.Count > 0 && _lines[^1].Trim().Length > 0)
            {
                _lines.Add(string.Empty);
            }
            _lines.Add($"[{section}]");
            _lines.Add($"{key}={value}");
            return;
        }
        // Après la dernière ligne non vide de la section.
        int insertAt = sectionLine + 1;
        for (int i = sectionLine + 1; i < _lines.Count && !IsSectionHeader(_lines[i]); i++)
        {
            if (_lines[i].Trim().Length > 0)
            {
                insertAt = i + 1;
            }
        }
        _lines.Insert(insertAt, $"{key}={value}");
    }

    public void SetBool(string section, string key, bool value) => Set(section, key, value ? "1" : "0");

    public void SetInt(string section, string key, int value) => Set(section, key, value.ToString());

    public void Save(string path) => File.WriteAllLines(path, _lines, new UTF8Encoding(false));

    public override string ToString() => string.Join("\n", _lines);

    private int FindSection(string section)
    {
        for (int i = 0; i < _lines.Count; i++)
        {
            if (IsSectionHeader(_lines[i]) &&
                string.Equals(_lines[i].Trim()[1..^1].Trim(), section, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }
        return -1;
    }

    private int FindKey(string section, string key)
    {
        int sectionLine = FindSection(section);
        if (sectionLine < 0)
        {
            return -1;
        }
        for (int i = sectionLine + 1; i < _lines.Count && !IsSectionHeader(_lines[i]); i++)
        {
            string line = _lines[i].Trim();
            if (line.StartsWith(';') || line.StartsWith('#'))
            {
                continue;
            }
            int equals = line.IndexOf('=');
            if (equals > 0 && string.Equals(line[..equals].Trim(), key, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }
        return -1;
    }

    private static bool IsSectionHeader(string line)
    {
        string trimmed = line.Trim();
        return trimmed.Length > 2 && trimmed[0] == '[' && trimmed[^1] == ']';
    }

    private static string ValueOf(string line) => line[(line.IndexOf('=') + 1)..].Trim();
}
