# Active (ou retire) le drapeau LARGE_ADDRESS_AWARE de DP.exe : le jeu, 32 bits, peut alors utiliser
# ~4 Go d'espace d'adressage au lieu de 2 Go. C'est le « patch 4 Go » des guides communautaires.
#
# MODIFIE DP.exe : une copie de l'original est faite (DP.exe.dpsf-original) avant la première
# modification. Une vérification de l'intégrité des fichiers par Steam annule ce patch.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\large-address-aware.ps1 -Status
#   powershell -ExecutionPolicy Bypass -File tools\large-address-aware.ps1 -Enable
#   powershell -ExecutionPolicy Bypass -File tools\large-address-aware.ps1 -Restore
param(
    [string]$GameDir = "E:\SteamLibrary\steamapps\common\Deadly Premonition The Director's Cut",
    [switch]$Status,
    [switch]$Enable,
    [switch]$Restore
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $GameDir 'DP.exe'
$backup = "$exe.dpsf-original"
$largeAddressAware = 0x20

function Get-CharacteristicsOffset([byte[]]$bytes) {
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ([BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) { throw 'DP.exe : en-tête PE invalide' }
    return $peOffset + 22  # IMAGE_FILE_HEADER.Characteristics
}

function Test-LargeAddressAware {
    $bytes = [IO.File]::ReadAllBytes($exe)
    $flags = [BitConverter]::ToUInt16($bytes, (Get-CharacteristicsOffset $bytes))
    return ($flags -band $largeAddressAware) -ne 0
}

if (-not (Test-Path $exe)) { throw "DP.exe introuvable dans $GameDir" }

if ($Restore) {
    if (-not (Test-Path $backup)) { throw "Aucune copie d'origine ($backup)" }
    Copy-Item $backup $exe -Force
    Write-Host 'DP.exe d''origine restauré.'
} elseif ($Enable) {
    if (Test-LargeAddressAware) {
        Write-Host 'Déjà actif : rien à faire.'
        return
    }
    if (-not (Test-Path $backup)) {
        Copy-Item $exe $backup
        Write-Host "Copie de l'original : $backup"
    }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $offset = Get-CharacteristicsOffset $bytes
    $flags = [BitConverter]::ToUInt16($bytes, $offset) -bor $largeAddressAware
    $newFlags = [BitConverter]::GetBytes([uint16]$flags)
    $bytes[$offset] = $newFlags[0]
    $bytes[$offset + 1] = $newFlags[1]
    [IO.File]::WriteAllBytes($exe, $bytes)
    Write-Host 'LARGE_ADDRESS_AWARE activé.'
}

Write-Host "LARGE_ADDRESS_AWARE : $(if (Test-LargeAddressAware) { 'actif' } else { 'inactif' })"
