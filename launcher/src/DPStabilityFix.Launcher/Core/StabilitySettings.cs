namespace DPStabilityFix.Launcher.Core;

/// <summary>Options de DPStabilityFix.ini présentées dans le launcher (mêmes valeurs par défaut que la DLL).</summary>
public sealed record StabilitySettings
{
    public bool AtomicSaveWrites { get; init; } = true;
    public int SaveBackupCount { get; init; } = 20;
    public bool CrashDumps { get; init; } = true;
    public bool FullMemoryDumps { get; init; }
    public bool PreciseGameTime { get; init; } = true;        // [Frames] ForceFpuPreserve
    public bool PreciseTimer { get; init; } = true;           // [Frames] TimerResolutionMs = 1
    public int FrameLimitFps { get; init; }
    public bool IntegratedDpfix { get; init; } = true;
    public bool FrameStats { get; init; } = true;

    public static StabilitySettings Read(IniDocument ini)
    {
        StabilitySettings defaults = new();
        return new StabilitySettings
        {
            AtomicSaveWrites = ini.GetBool("Save", "AtomicWrites", defaults.AtomicSaveWrites),
            SaveBackupCount = ini.GetInt("Save", "BackupCount", defaults.SaveBackupCount),
            CrashDumps = ini.GetBool("Crash", "CrashDumps", defaults.CrashDumps),
            FullMemoryDumps = ini.GetBool("Crash", "FullMemoryDumps", defaults.FullMemoryDumps),
            PreciseGameTime = ini.GetBool("Frames", "ForceFpuPreserve", defaults.PreciseGameTime),
            PreciseTimer = ini.GetInt("Frames", "TimerResolutionMs", 1) > 0,
            FrameLimitFps = ini.GetInt("Frames", "FrameLimitFps", defaults.FrameLimitFps),
            IntegratedDpfix = ini.GetBool("Graphics", "IntegratedDPfix", defaults.IntegratedDpfix),
            FrameStats = ini.GetBool("Frames", "FrameStats", defaults.FrameStats),
        };
    }

    public void Write(IniDocument ini)
    {
        ini.SetBool("Save", "AtomicWrites", AtomicSaveWrites);
        ini.SetInt("Save", "BackupCount", Math.Clamp(SaveBackupCount, 1, 500));
        ini.SetBool("Crash", "CrashDumps", CrashDumps);
        ini.SetBool("Crash", "FullMemoryDumps", FullMemoryDumps);
        ini.SetBool("Frames", "ForceFpuPreserve", PreciseGameTime);
        ini.SetInt("Frames", "TimerResolutionMs", PreciseTimer ? 1 : 0);
        ini.SetInt("Frames", "FrameLimitFps", Math.Clamp(FrameLimitFps, 0, 1000));
        ini.SetBool("Graphics", "IntegratedDPfix", IntegratedDpfix);
        ini.SetBool("Frames", "FrameStats", FrameStats);
    }
}
