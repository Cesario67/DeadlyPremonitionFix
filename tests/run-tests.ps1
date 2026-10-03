# Tests d'intégration de DPStabilityFix avec le faux DP.exe (tests/fake_game).
# Prérequis : tools\build.ps1 (compile la DLL et le faux jeu).
# Usage : powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1 [-Preset x86-release]
#        -SkipSystemDependent : pour la CI, sans GPU ni runtime DirectX juin 2010 (X3DAudio1_7.dll).
param(
    [string]$Preset = 'x86-release',
    [switch]$SkipSystemDependent
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$gameDir = Join-Path $root "build\$Preset\fake_game"
$exe = Join-Path $gameDir 'DP.exe'
if (-not (Test-Path $exe)) {
    throw "Faux jeu introuvable ($exe) : lancer d'abord tools\build.ps1"
}

$script:failures = 0
$save = Join-Path $gameDir 'savedata\dp.sav'
$modDir = Join-Path $gameDir 'DPStabilityFix'

function Reset-Environment([string[]]$extraFrameSettings = @()) {
    foreach ($path in @($modDir, (Join-Path $gameDir 'savedata'), (Join-Path $gameDir 'gamefilter.txt'),
                        (Join-Path $gameDir 'timer-precision.txt'), (Join-Path $gameDir 'reset-result.txt'),
                        (Join-Path $gameDir 'DPfix.ini'))) {
        if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    }
    # Rapports fréquents pour le test de cadence.
    Set-Content -Path (Join-Path $gameDir 'DPStabilityFix.ini') -Encoding Unicode -Value (@(
        '[Frames]', 'ReportIntervalSeconds=2'
    ) + $extraFrameSettings)
}

# Lit timer-precision.txt écrit par le scénario « frames » : précision x87 et pas du temps du jeu.
function Read-TimerPrecision {
    $path = Join-Path $gameDir 'timer-precision.txt'
    if (-not (Test-Path $path)) { return $null }
    $text = Get-Content $path -Raw
    if ($text -match 'x87=(\d+) step_ms=([\d.]+)') {
        return @{ Bits = [int]$Matches[1]; StepMs = [double]::Parse($Matches[2], [Globalization.CultureInfo]::InvariantCulture) }
    }
    return $null
}

function Invoke-FakeGame([string]$scenario, [int]$timeoutSeconds = 60) {
    $process = Start-Process -FilePath $exe -ArgumentList $scenario -WorkingDirectory $gameDir -PassThru -WindowStyle Hidden
    $null = $process.Handle  # sans cela, ExitCode reste vide après WaitForExit
    # Un blocage (boucle infinie, Reset sans fin...) doit faire échouer le test, pas le figer.
    if (-not $process.WaitForExit($timeoutSeconds * 1000)) {
        $process.Kill()
        Write-Host "        délai dépassé ($timeoutSeconds s) pour le scénario $scenario" -ForegroundColor Yellow
        return -999
    }
    return $process.ExitCode
}

function Get-LatestLog {
    $log = Get-ChildItem (Join-Path $modDir 'logs') -Filter 'DPStabilityFix_*.log' | Sort-Object Name | Select-Object -Last 1
    if (-not $log) { return '' }
    return [IO.File]::ReadAllText($log.FullName, [Text.Encoding]::UTF8)
}

function Assert([string]$name, [bool]$condition) {
    if ($condition) {
        Write-Host "  OK    $name" -ForegroundColor Green
    } else {
        Write-Host "  ECHEC $name" -ForegroundColor Red
        $script:failures++
    }
}

function Read-Save {
    if (Test-Path $save) { return [IO.File]::ReadAllText($save) }
    return '<absent>'
}

Write-Host "Initialisation et proxy X3DAudio"
Reset-Environment
$code = Invoke-FakeGame $(if ($SkipSystemDependent) { 'save' } else { 'audio' })
$log = Get-LatestLog
if (-not $SkipSystemDependent) {
    Assert 'X3DAudioInitialize transmis à la vraie DLL' ($code -eq 0)
}
Assert 'journal créé et mod initialisé' ($log -match 'Initialisation terminée')
Assert 'IAT de kernel32 interceptée sans erreur' (-not ($log -match 'Interception de .* impossible'))

Write-Host "Écritures de sauvegarde"
Reset-Environment
$code = Invoke-FakeGame 'save'
$log = Get-LatestLog
Assert 'le faux jeu relit ce qu''il a écrit' ($code -eq 0)
Assert 'contenu final de dp.sav' ((Read-Save) -eq 'partie-v2-plus-longue')
Assert 'écritures redirigées vers la copie temporaire' ($log -match 'Écriture redirigée')
Assert 'deux sauvegardes validées' (([regex]::Matches($log, 'Sauvegarde validée')).Count -eq 2)
Assert 'point de contrôle sur FlushFileBuffers' ($log -match 'Point de contrôle de la sauvegarde')
Assert 'aucun fichier temporaire restant' (-not (Test-Path "$save.dpsf.tmp"))
Assert 'copie de secours avant la 2e écriture' ((Get-ChildItem (Join-Path $modDir 'savebackups') -Filter 'dp_*_avant-ecriture.sav').Count -ge 1)

Write-Host "Plantage pendant une écriture"
Reset-Environment
$null = Invoke-FakeGame 'save-crash'
Assert 'dp.sav intact après le plantage' ((Read-Save) -eq 'partie-stable')
Assert 'écriture interrompue restée dans le fichier temporaire' (Test-Path "$save.dpsf.tmp")
$null = Invoke-FakeGame 'unknown-scenario'  # simple lancement : le mod s'initialise puis le faux jeu quitte
$log = Get-LatestLog
Assert 'fichier temporaire récupéré au lancement suivant' (-not (Test-Path "$save.dpsf.tmp"))
Assert 'récupération journalisée' ($log -match 'Écriture de sauvegarde interrompue')
Assert 'fichier incomplet conservé pour analyse' ((Get-ChildItem (Join-Path $modDir 'savebackups') -Filter 'recovered_*.sav').Count -eq 1)
Assert 'dp.sav toujours intact' ((Read-Save) -eq 'partie-stable')

Write-Host "Fermeture du jeu avec la sauvegarde ouverte"
Reset-Environment
$null = Invoke-FakeGame 'save-exit'
$log = Get-LatestLog
Assert 'données publiées à la fermeture' ((Read-Save) -eq 'partie-ecrite-a-la-sortie')
Assert 'fermeture journalisée' ($log -match 'Fermeture normale du jeu')

Write-Host "Suppression de la sauvegarde par le jeu"
Reset-Environment
$code = Invoke-FakeGame 'save-delete'
Assert 'suppression autorisée' ($code -eq 0 -and -not (Test-Path $save))
$backup = Get-ChildItem (Join-Path $modDir 'savebackups') -Filter 'dp_*_avant-suppression.sav' | Select-Object -First 1
Assert 'copie de secours avant suppression' ($null -ne $backup -and [IO.File]::ReadAllText($backup.FullName) -eq 'partie-a-supprimer')

Write-Host "Plantage du jeu"
Reset-Environment
$null = Invoke-FakeGame 'crash'
$log = Get-LatestLog
Assert 'plantage journalisé' ($log -match 'PLANTAGE : EXCEPTION_ACCESS_VIOLATION')
Assert 'adresse fautive située dans DP.exe' ($log -match 'Adresse : DP\.exe\+0x')
Assert 'fichier de diagnostic écrit' ((Get-ChildItem (Join-Path $modDir 'crashdumps') -Filter 'DP_*.dmp').Count -eq 1)
Assert 'gestionnaire du jeu appelé après le nôtre' (Test-Path (Join-Path $gameDir 'gamefilter.txt'))

Write-Host "Installeur (patch 4 Go + copie du mod)"
$setup = Join-Path $root "build\$Preset\package\DPStabilityFixSetup.exe"
$setupGame = Join-Path $root "build\$Preset\setup_test"
if (Test-Path $setupGame) { Remove-Item -Recurse -Force $setupGame }
New-Item -ItemType Directory $setupGame | Out-Null
Copy-Item $exe $setupGame
$setupExe = Join-Path $setupGame 'DP.exe'

function Invoke-Setup([string]$target) {
    $process = Start-Process -FilePath $setup -ArgumentList "`"$target`"", '--quiet' -PassThru
    $null = $process.Handle
    if (-not $process.WaitForExit(60000)) { $process.Kill(); return -999 }
    return $process.ExitCode
}
function Test-LargeAddressAware([string]$path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    return ([BitConverter]::ToUInt16($bytes, $pe + 22) -band 0x20) -ne 0
}

Assert 'faux DP.exe sans patch 4 Go au départ' (-not (Test-LargeAddressAware $setupExe))
Assert 'installation via le dossier du jeu' ((Invoke-Setup $setupGame) -eq 0)
Assert 'patch 4 Go appliqué' (Test-LargeAddressAware $setupExe)
Assert 'original conservé sans le patch' ((Test-Path "$setupExe.dpsf-original") -and -not (Test-LargeAddressAware "$setupExe.dpsf-original"))
Assert 'DLL du mod copiée' (Test-Path (Join-Path $setupGame 'X3DAudio1_7.dll'))
Assert '.ini copié' (Test-Path (Join-Path $setupGame 'DPStabilityFix.ini'))
Assert 'aucun fichier temporaire restant' (-not (Test-Path "$setupExe.dpsf-tmp"))
Assert 'DPfix.ini et DPfixKeys.ini installés' ((Test-Path (Join-Path $setupGame 'DPfix.ini')) -and (Test-Path (Join-Path $setupGame 'DPfixKeys.ini')))
Assert 'shaders de DPfix installés' (Test-Path (Join-Path $setupGame 'dpfix\SMAA.fx'))
Assert 'DPfix.ini livré sans désactivation de la manette' ((Get-Content (Join-Path $setupGame 'DPfix.ini')) -contains 'disableJoystick 0')
$originalHash = (Get-FileHash "$setupExe.dpsf-original").Hash
Assert 'réinstallation via DP.exe directement' ((Invoke-Setup $setupExe) -eq 0)
Assert 'copie d''origine inchangée après réinstallation' ((Get-FileHash "$setupExe.dpsf-original").Hash -eq $originalHash)
Assert 'refus d''un autre fichier que DP.exe' ((Invoke-Setup (Join-Path $setupGame 'X3DAudio1_7.dll')) -ne 0)
Set-Content -Path (Join-Path $setupGame 'd3d9.dll') -Value 'faux DPfix d origine'
$null = Invoke-Setup $setupGame
Assert 'DPfix d''origine conservé sans accord' (Test-Path (Join-Path $setupGame 'd3d9.dll'))
$process = Start-Process -FilePath $setup -ArgumentList "`"$setupGame`"", '--quiet', '--disable-external-dpfix' -PassThru
$null = $process.Handle; $null = $process.WaitForExit(60000)
Assert 'DPfix d''origine renommé sur demande' (-not (Test-Path (Join-Path $setupGame 'd3d9.dll')) -and (Test-Path (Join-Path $setupGame 'd3d9.dll.dpfix-desactive')))
$lock = [IO.File]::Open($setupExe, 'Open', 'Read', 'Read')
try {
    Assert 'refus si le jeu est lancé' ((Invoke-Setup $setupGame) -ne 0)
} finally {
    $lock.Close()
}
Assert 'le DP.exe patché démarre toujours' ((Start-Process -FilePath $setupExe -ArgumentList 'unknown-scenario' -Wait -PassThru -WindowStyle Hidden).ExitCode -eq 2)
$setupLog = Get-ChildItem (Join-Path $setupGame 'DPStabilityFix\logs') -Filter '*.log' | Sort-Object Name | Select-Object -Last 1
$setupLogText = if ($setupLog) { [IO.File]::ReadAllText($setupLog.FullName, [Text.Encoding]::UTF8) } else { '' }
Assert 'le processus patché dispose de ~4 Go d''espace d''adressage' ($setupLogText -match 'Espace d''adressage du processus : 40\d\d Mo')

if (-not $SkipSystemDependent) {
    Write-Host "Direct3D 9 et cadence"
    Reset-Environment
    $code = Invoke-FakeGame 'frames'
    $log = Get-LatestLog
    Assert 'le faux jeu a présenté ses images' ($code -eq 0)
    Assert 'Present intercepté' ($log -match 'Present intercepté')
    Assert 'rapports de cadence écrits' ($log -match 'Images : ')
    Assert 'changement de thread de Present suivi' ($log -match 'Present appelé depuis un nouveau thread')
    Assert 'Sleep du thread de rendu mesuré' ($log -match 'Sleep thread de rendu : \d+ appels')
    Assert 'résolution du minuteur appliquée' ($log -match 'timeBeginPeriod\(1\)')
    Assert 'FPU_PRESERVE ajouté à CreateDevice' ($log -match 'D3DCREATE_FPU_PRESERVE ajouté')
    Assert 'DPfix intégré actif (sans DPfix.ini)' ($log -match 'DPfix intégré actif')
    $precision = Read-TimerPrecision
    Assert 'x87 en pleine précision après CreateDevice (mod actif)' ($null -ne $precision -and $precision.Bits -ge 53)
    Assert 'temps « façon DP.exe » précis à 7 jours de démarrage (< 0,1 ms)' ($null -ne $precision -and $precision.StepMs -gt 0 -and $precision.StepMs -lt 0.1)
    if ($precision) { Write-Host ("        mod actif : x87 {0} bits, pas du temps {1} ms" -f $precision.Bits, $precision.StepMs) }

    Write-Host "Direct3D 9 sans FPU_PRESERVE (comportement d'origine du jeu)"
    Reset-Environment @('ForceFpuPreserve=0')
    $null = Invoke-FakeGame 'frames'
    $precision = Read-TimerPrecision
    Assert 'Direct3D passe le x87 en simple précision' ($null -ne $precision -and $precision.Bits -eq 24)
    Assert 'temps « façon DP.exe » dégradé à 7 jours de démarrage (> 30 ms)' ($null -ne $precision -and $precision.StepMs -gt 30)
    if ($precision) { Write-Host ("        sans correctif : x87 {0} bits, pas du temps {1} ms" -f $precision.Bits, $precision.StepMs) }

    # Paramètres du jeu (plein écran 59/60 Hz) convertis en fenêtré par DPfix, rendu avec une cible de
    # rendu affichée comme texture, puis Reset : reproduit les blocages et le plantage observés en jeu.
    foreach ($mode in @('forceWindowed 1', 'borderlessFullscreen 1')) {
        Write-Host "DPfix intégré ($mode) : paramètres du jeu, rendu puis Reset"
        Reset-Environment
        Set-Content -Path (Join-Path $gameDir 'DPfix.ini') -Encoding Ascii -Value @(
            'renderWidth 320', 'renderHeight 240', 'presentWidth 320', 'presentHeight 240',
            $mode, 'aaQuality 1', 'aaType SMAA', 'logLevel 1'
        )
        $code = Invoke-FakeGame 'dpfix-reset'
        $log = Get-LatestLog
        $resetResult = if (Test-Path (Join-Path $gameDir 'reset-result.txt')) { (Get-Content (Join-Path $gameDir 'reset-result.txt') -Raw).Trim() } else { '<absent>' }
        Assert 'DPfix intégré chargé avec DPfix.ini (SMAA)' ($log -match 'DPfix intégré \(0\.9 corrigé\) : rendu 320x240.*AA 1')
        Assert 'CreateDevice réussi avec les paramètres du jeu (59 Hz)' ($log -match 'CreateDevice \(.*\) -> 0x00000000')
        Assert 'Reset réussi après rendu' ($code -eq 0 -and $resetResult -eq 'reset=0x00000000')
        Assert 'journal interne de DPfix actif (logLevel 1)' ($log -match '\[DPfix\] Reset ------')
        Write-Host "        $resetResult"
    }

    Write-Host "DPfix d'origine présent (d3d9.dll externe)"
    Reset-Environment
    $externalD3d9 = Join-Path $gameDir 'd3d9.dll'
    Copy-Item (Join-Path $env:SystemRoot 'SysWOW64\d3d9.dll') $externalD3d9
    try {
        $code = Invoke-FakeGame 'frames'
        $log = Get-LatestLog
        Assert 'd3d9.dll local détecté' ($log -match 'd3d9.dll chargé : .*fake_game\\d3d9.dll \(DLL locale')
        Assert 'DPfix intégré désactivé (pas de double traitement)' (($log -match 'DPfix intégré désactivé') -and -not ($log -match 'DPfix intégré actif'))
        Assert 'le jeu fonctionne toujours' ($code -eq 0)
    } finally {
        Remove-Item $externalD3d9 -Force
    }
}

Write-Host ''
if ($script:failures -gt 0) {
    Write-Host "$($script:failures) test(s) en échec. Journal : $modDir\logs" -ForegroundColor Red
    exit 1
}
Write-Host 'Tous les tests passent.' -ForegroundColor Green
