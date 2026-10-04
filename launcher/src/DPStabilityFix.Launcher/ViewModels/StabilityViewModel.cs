using CommunityToolkit.Mvvm.ComponentModel;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Onglet Stabilité : options de DPStabilityFix.ini.</summary>
public sealed partial class StabilityViewModel : ObservableObject
{
    [ObservableProperty]
    public partial bool AtomicSaveWrites { get; set; } = true;

    [ObservableProperty]
    public partial decimal SaveBackupCount { get; set; } = 20;

    [ObservableProperty]
    public partial bool CrashDumps { get; set; } = true;

    [ObservableProperty]
    public partial bool FullMemoryDumps { get; set; }

    [ObservableProperty]
    public partial bool PreciseGameTime { get; set; } = true;

    [ObservableProperty]
    public partial bool PreciseTimer { get; set; } = true;

    [ObservableProperty]
    public partial decimal FrameLimitFps { get; set; }

    [ObservableProperty]
    public partial bool IntegratedDpfix { get; set; } = true;

    [ObservableProperty]
    public partial bool FrameStats { get; set; } = true;

    public void Load(IniDocument ini)
    {
        StabilitySettings settings = StabilitySettings.Read(ini);
        AtomicSaveWrites = settings.AtomicSaveWrites;
        SaveBackupCount = settings.SaveBackupCount;
        CrashDumps = settings.CrashDumps;
        FullMemoryDumps = settings.FullMemoryDumps;
        PreciseGameTime = settings.PreciseGameTime;
        PreciseTimer = settings.PreciseTimer;
        FrameLimitFps = settings.FrameLimitFps;
        IntegratedDpfix = settings.IntegratedDpfix;
        FrameStats = settings.FrameStats;
    }

    public void Save(IniDocument ini) => new StabilitySettings
    {
        AtomicSaveWrites = AtomicSaveWrites,
        SaveBackupCount = (int)SaveBackupCount,
        CrashDumps = CrashDumps,
        FullMemoryDumps = FullMemoryDumps,
        PreciseGameTime = PreciseGameTime,
        PreciseTimer = PreciseTimer,
        FrameLimitFps = (int)FrameLimitFps,
        IntegratedDpfix = IntegratedDpfix,
        FrameStats = FrameStats,
    }.Write(ini);
}
