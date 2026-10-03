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

function Reset-Environment {
    foreach ($path in @($modDir, (Join-Path $gameDir 'savedata'), (Join-Path $gameDir 'gamefilter.txt'))) {
        if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    }
    # Rapports fréquents pour le test de cadence.
    Set-Content -Path (Join-Path $gameDir 'DPStabilityFix.ini') -Encoding Unicode -Value @(
        '[Frames]', 'ReportIntervalSeconds=2'
    )
}

function Invoke-FakeGame([string]$scenario) {
    $process = Start-Process -FilePath $exe -ArgumentList $scenario -WorkingDirectory $gameDir -Wait -PassThru -WindowStyle Hidden
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

if (-not $SkipSystemDependent) {
    Write-Host "Direct3D 9 et cadence"
    Reset-Environment
    $code = Invoke-FakeGame 'frames'
    $log = Get-LatestLog
    Assert 'le faux jeu a présenté ses images' ($code -eq 0)
    Assert 'Present intercepté' ($log -match 'Present intercepté')
    Assert 'rapports de cadence écrits' ($log -match 'Images : ')
    Assert 'Sleep du thread de rendu mesuré' ($log -match 'Sleep thread de rendu : \d+ appels')
    Assert 'résolution du minuteur appliquée' ($log -match 'timeBeginPeriod\(1\)')
}

Write-Host ''
if ($script:failures -gt 0) {
    Write-Host "$($script:failures) test(s) en échec. Journal : $modDir\logs" -ForegroundColor Red
    exit 1
}
Write-Host 'Tous les tests passent.' -ForegroundColor Green
