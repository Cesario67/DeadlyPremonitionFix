# Compile DPStabilityFix en 32 bits avec les Build Tools, sans ouvrir de terminal développeur.
# Usage : powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Preset x86-release|x86-debug]
param(
    [ValidateSet('x86-release', 'x86-debug')]
    [string]$Preset = 'x86-release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'Build Tools introuvables : installer Microsoft.VisualStudio.2022.BuildTools avec la charge VCTools.'
}
$vsDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsDir) {
    throw 'Aucune installation avec le compilateur C++ (charge VCTools) trouvée.'
}
$vcvars = Join-Path $vsDir 'VC\Auxiliary\Build\vcvarsall.bat'

# vcvarsall prépare l'environnement du compilateur 32 bits (hôte x64, cible x86) pour cette commande.
cmd /c "`"$vcvars`" amd64_x86 >nul && cd /d `"$root`" && cmake --preset $Preset && cmake --build --preset $Preset"
if ($LASTEXITCODE -ne 0) {
    throw "Échec de la compilation (code $LASTEXITCODE)."
}

Write-Host ''
Write-Host "DLL : $root\build\$Preset\X3DAudio1_7.dll"
