using CommunityToolkit.Mvvm.ComponentModel;
using DPStabilityFix.Launcher.Core;

namespace DPStabilityFix.Launcher.ViewModels;

/// <summary>Onglet Stabilité : options de DPStabilityFix.ini.</summary>
public sealed partial class StabilityViewModel : LocalizedViewModel
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
    public partial bool AimPrecisionGuard { get; set; } = true;

    [ObservableProperty]
    public partial bool SonyControllerLayout { get; set; } = true;

    [ObservableProperty]
    public partial bool SwapControllerTriggers { get; set; }

    [ObservableProperty]
    public partial bool BackgroundControllerPolling { get; set; } = true;

    [ObservableProperty]
    public partial bool PreciseTimer { get; set; } = true;

    [ObservableProperty]
    public partial decimal FrameLimitFps { get; set; } = 60;

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
        AimPrecisionGuard = settings.AimPrecisionGuard;
        SonyControllerLayout = settings.SonyControllerLayout;
        SwapControllerTriggers = settings.SwapControllerTriggers;
        BackgroundControllerPolling = settings.BackgroundControllerPolling;
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
        AimPrecisionGuard = AimPrecisionGuard,
        SonyControllerLayout = SonyControllerLayout,
        SwapControllerTriggers = SwapControllerTriggers,
        BackgroundControllerPolling = BackgroundControllerPolling,
        PreciseTimer = PreciseTimer,
        FrameLimitFps = (int)FrameLimitFps,
        IntegratedDpfix = IntegratedDpfix,
        FrameStats = FrameStats,
    }.Write(ini);
}
