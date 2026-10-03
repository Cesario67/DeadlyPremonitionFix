# Installe DPStabilityFix dans le dossier du jeu (DLL + .ini). Ne modifie aucun fichier du jeu.
# Usage : powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 [-GameDir "..."] [-Preset x86-release]
param(
    [string]$GameDir = "E:\SteamLibrary\steamapps\common\Deadly Premonition The Director's Cut",
    [string]$Preset = 'x86-release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root "build\$Preset\X3DAudio1_7.dll"
$ini = Join-Path $root 'dist\DPStabilityFix.ini'

if (-not (Test-Path (Join-Path $GameDir 'DP.exe'))) {
    throw "DP.exe introuvable dans $GameDir"
}
if (-not (Test-Path $dll)) {
    throw "DLL introuvable ($dll) : lancer d'abord tools\build.ps1"
}

$target = Join-Path $GameDir 'X3DAudio1_7.dll'
if (Test-Path $target) {
    $description = (Get-Item $target).VersionInfo.FileDescription
    if ($description -and $description -notmatch 'DPStabilityFix') {
        # Une autre X3DAudio1_7.dll était déjà là : on la met de côté plutôt que de l'écraser.
        Copy-Item $target "$target.avant-dpsf" -Force
        Write-Host "X3DAudio1_7.dll existante conservée sous X3DAudio1_7.dll.avant-dpsf"
    }
}
Copy-Item $dll $target -Force
Write-Host "Installé : $target"

$targetIni = Join-Path $GameDir 'DPStabilityFix.ini'
if (Test-Path $targetIni) {
    Write-Host "DPStabilityFix.ini déjà présent : conservé (modèle à jour dans dist\)"
} else {
    Copy-Item $ini $targetIni
    Write-Host "Installé : $targetIni"
}
