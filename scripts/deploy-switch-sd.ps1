[CmdletBinding()]
param(
    # Destination SD-card root: a card-reader drive (e.g. E:\), a staging folder,
    # or the Yuzu "SD Card" directory. Files land under <root>\switch\voxelgame\.
    [Parameter(Mandatory = $true)]
    [string]$SdRoot,

    # Path to the built .nro. Defaults to the local Switch release build.
    [string]$Nro = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$assets = Join-Path $repoRoot "assets"
if (-not (Test-Path -LiteralPath $assets)) {
    throw "assets folder not found at $assets"
}
if (-not $Nro) {
    $Nro = Join-Path $repoRoot "build\switch-release\voxelgame.nro"
}
if (-not (Test-Path -LiteralPath $Nro)) {
    throw "NRO not found at $Nro (build it with scripts\build-switch.ps1, or pass -Nro)"
}

$target = Join-Path $SdRoot "switch\voxelgame"
New-Item -ItemType Directory -Force -Path (Join-Path $target "assets") | Out-Null

Copy-Item -LiteralPath $Nro -Destination (Join-Path $target "voxelgame.nro") -Force
Copy-Item -Path (Join-Path $assets "*") -Destination (Join-Path $target "assets") -Recurse -Force

Write-Host "Deployed to $target :"
Get-ChildItem -Recurse -File $target | ForEach-Object {
    Write-Host "  " ($_.FullName.Substring($SdRoot.Length).TrimStart('\'))
}
Write-Host "On the console: hbmenu -> voxelgame -> voxelgame.nro"
