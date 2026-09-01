[CmdletBinding()]
param(
    # Destination SD-card root. Defaults to the emulator's SD directory
    # (yuzu: Settings -> Filesystem -> "SD Card"). Files land under
    # <root>\switch\<appName>\, the same layout a real console uses -- so
    # copying <root>\switch\<appName>\ onto the console SD is the whole port.
    [string]$SdRoot = "D:\SDMC",

    # Path to the built .nro. Defaults to the local Switch release build
    # (voxeltactics.nro if present, else voxelgame.nro).
    [string]$Nro = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$assets = Join-Path $repoRoot "assets"
if (-not (Test-Path -LiteralPath $assets)) {
    throw "assets folder not found at $assets"
}
if (-not $Nro) {
    $buildDir = Join-Path $repoRoot "build\switch-release"
    $tactics = Join-Path $buildDir "voxeltactics.nro"
    $sandbox = Join-Path $buildDir "voxelgame.nro"
    $Nro = if (Test-Path -LiteralPath $tactics) { $tactics } else { $sandbox }
}
if (-not (Test-Path -LiteralPath $Nro)) {
    throw "NRO not found at $Nro (build it with scripts\build-switch.ps1, or pass -Nro)"
}

# The battle NRO ships no assets in its romfs, so this deploy is mandatory:
# models, atlases, animations and Lua scripts are all read from the SD card.
# The hbmenu / SD folder name matches the NRO so titles can coexist.
$appName = [System.IO.Path]::GetFileNameWithoutExtension($Nro)
$target = Join-Path $SdRoot "switch\$appName"
New-Item -ItemType Directory -Force -Path (Join-Path $target "assets") | Out-Null

Copy-Item -LiteralPath $Nro -Destination (Join-Path $target "$appName.nro") -Force
Copy-Item -Path (Join-Path $assets "*") -Destination (Join-Path $target "assets") -Recurse -Force

Write-Host "Deployed to $target :"
Get-ChildItem -Recurse -File $target | ForEach-Object {
    Write-Host "  " ($_.FullName.Substring($SdRoot.Length).TrimStart('\'))
}
Write-Host ""
Write-Host "Emulator: add '$target' as a yuzu game directory (or run the .nro from it)."
Write-Host "Console : copy '$target' to <SD>\switch\$appName\"
