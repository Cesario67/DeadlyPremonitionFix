using System.Buffers.Binary;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.Tests;

/// <summary>Dossier de jeu temporaire avec un faux DP.exe (en-tête PE minimal), supprimé après le test.</summary>
internal sealed class TestGame : IDisposable
{
    public TestGame(uint timestamp = ExeInfo.KnownGameTimestamp, bool largeAddressAware = false)
    {
        Directory.CreateDirectory(Root);
        File.WriteAllBytes(Paths.GameExe, MinimalExe(timestamp, largeAddressAware));
    }

    public string Root { get; } = Path.Combine(Path.GetTempPath(), "dpsf-tests", Guid.NewGuid().ToString("N"));

    public GamePaths Paths => new(Root);

    public void Dispose()
    {
        if (Directory.Exists(Root))
        {
            Directory.Delete(Root, recursive: true);
        }
    }

    /// <summary>En-tête DOS + PE suffisant pour ExePatcher (machine i386, horodatage, caractéristiques).</summary>
    public static byte[] MinimalExe(uint timestamp, bool largeAddressAware)
    {
        byte[] bytes = new byte[512];
        bytes[0] = (byte)'M';
        bytes[1] = (byte)'Z';
        const int peOffset = 0x80;
        BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(0x3C), peOffset);
        "PE\0\0"u8.CopyTo(bytes.AsSpan(peOffset));
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(peOffset + 4), 0x014C);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(peOffset + 8), timestamp);
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(peOffset + 22), (ushort)(0x0102 | (largeAddressAware ? 0x20 : 0)));
        return bytes;
    }
}
