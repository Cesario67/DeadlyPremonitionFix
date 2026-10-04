using System.Buffers.Binary;

namespace DPStabilityFix.Launcher.Core;

/// <summary>Informations lues dans l'en-tête PE de DP.exe.</summary>
public sealed record ExeInfo(bool IsValid, bool IsX86, uint Timestamp, bool LargeAddressAware)
{
    public static readonly ExeInfo Invalid = new(false, false, 0, false);

    /// <summary>Version Steam 1.01b, celle pour laquelle le mod a été conçu.</summary>
    public const uint KnownGameTimestamp = 0x529721DC;

    public bool IsKnownVersion => Timestamp == KnownGameTimestamp;
}

/// <summary>
/// Patch « 4 Go » : active LARGE_ADDRESS_AWARE dans l'en-tête de DP.exe pour que le jeu (32 bits) dispose
/// d'environ 4 Go d'espace d'adressage au lieu de 2. L'original est copié une seule fois, et le fichier est
/// remplacé de façon atomique.
/// </summary>
public static class ExePatcher
{
    private const ushort MachineI386 = 0x014C;
    private const ushort LargeAddressAwareFlag = 0x0020;

    public static ExeInfo ReadInfo(string exePath)
    {
        if (!File.Exists(exePath))
        {
            return ExeInfo.Invalid;
        }
        byte[] header = ReadHeader(exePath);
        int fileHeader = FileHeaderOffset(header);
        if (fileHeader < 0)
        {
            return ExeInfo.Invalid;
        }
        ushort machine = BinaryPrimitives.ReadUInt16LittleEndian(header.AsSpan(fileHeader));
        uint timestamp = BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(fileHeader + 4));
        ushort characteristics = BinaryPrimitives.ReadUInt16LittleEndian(header.AsSpan(fileHeader + 18));
        return new ExeInfo(true, machine == MachineI386, timestamp, (characteristics & LargeAddressAwareFlag) != 0);
    }

    /// <summary>Renvoie true si le patch a été appliqué, false s'il l'était déjà.</summary>
    public static bool EnableLargeAddressAware(string exePath)
    {
        byte[] bytes = File.ReadAllBytes(exePath);
        int fileHeader = FileHeaderOffset(bytes);
        if (fileHeader < 0)
        {
            throw new InvalidDataException("DP.exe n'a pas un en-tête d'exécutable valide.");
        }
        Span<byte> characteristics = bytes.AsSpan(fileHeader + 18, 2);
        ushort flags = BinaryPrimitives.ReadUInt16LittleEndian(characteristics);
        if ((flags & LargeAddressAwareFlag) != 0)
        {
            return false;
        }

        // Copie de l'original une seule fois : une réinstallation ne doit pas écraser la vraie copie d'origine.
        string backup = exePath + ".dpsf-original";
        if (!File.Exists(backup))
        {
            File.Copy(exePath, backup);
        }

        BinaryPrimitives.WriteUInt16LittleEndian(characteristics, (ushort)(flags | LargeAddressAwareFlag));
        ReplaceAtomically(exePath, bytes);
        if (!ReadInfo(exePath).LargeAddressAware)
        {
            throw new IOException("Patch 4 Go : vérification échouée après écriture.");
        }
        return true;
    }

    /// <summary>Remet DP.exe d'origine à partir de la copie faite avant le premier patch.</summary>
    public static void RestoreOriginal(string exePath)
    {
        string backup = exePath + ".dpsf-original";
        if (!File.Exists(backup))
        {
            throw new FileNotFoundException("Aucune copie d'origine de DP.exe (DP.exe.dpsf-original).", backup);
        }
        ReplaceAtomically(exePath, File.ReadAllBytes(backup));
    }

    private static void ReplaceAtomically(string path, byte[] content)
    {
        string temporary = path + ".dpsf-tmp";
        using (FileStream stream = new(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
        {
            stream.Write(content);
            stream.Flush(flushToDisk: true);
        }
        File.Move(temporary, path, overwrite: true);
    }

    private static byte[] ReadHeader(string path)
    {
        using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        byte[] buffer = new byte[Math.Min(4096, stream.Length)];
        stream.ReadExactly(buffer);
        return buffer;
    }

    /// <summary>Offset de IMAGE_FILE_HEADER, ou -1 si l'en-tête est invalide.</summary>
    private static int FileHeaderOffset(byte[] bytes)
    {
        if (bytes.Length < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z')
        {
            return -1;
        }
        int peOffset = BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(0x3C));
        if (peOffset <= 0 || peOffset + 24 > bytes.Length)
        {
            return -1;
        }
        bool signature = bytes[peOffset] == 'P' && bytes[peOffset + 1] == 'E' && bytes[peOffset + 2] == 0 &&
                         bytes[peOffset + 3] == 0;
        return signature ? peOffset + 4 : -1;
    }
}
