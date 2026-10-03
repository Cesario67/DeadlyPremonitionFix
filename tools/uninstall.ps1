# Retire DPStabilityFix du dossier du jeu. Les journaux, diagnostics et copies de secours
# (dossier DPStabilityFix\) sont conservés.
# Usage : powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1 [-GameDir "..."] [-RemoveIni]
param(
    [string]$GameDir = "E:\SteamLibrary\steamapps\common\Deadly Premonition The Director's Cut",
    [switch]$RemoveIni
)

$ErrorActionPreference = 'Stop'
$target = Join-Path $GameDir 'X3DAudio1_7.dll'

if (Test-Path $target) {
    Remove-Item $target
    Write-Host "Retiré : $target"
}
if (Test-Path "$target.avant-dpsf") {
    Move-Item "$target.avant-dpsf" $target
    Write-Host 'X3DAudio1_7.dll d''origine restaurée'
}
if ($RemoveIni -and (Test-Path (Join-Path $GameDir 'DPStabilityFix.ini'))) {
    Remove-Item (Join-Path $GameDir 'DPStabilityFix.ini')
    Write-Host 'Retiré : DPStabilityFix.ini'
}
Write-Host "Données du mod conservées dans $(Join-Path $GameDir 'DPStabilityFix')"
