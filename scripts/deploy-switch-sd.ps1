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
    $buildDir = Join-Path $repoRoot "build\switch-release"
    # The tactics build ships as voxeltactics.nro; the sandbox build as voxelgame.nro.
    $tactics = Join-Path $buildDir "voxeltactics.nro"
    $sandbox = Join-Path $buildDir "voxelgame.nro"
    $Nro = if (Test-Path -LiteralPath $tactics) { $tactics } else { $sandbox }
}
if (-not (Test-Path -LiteralPath $Nro)) {
    throw "NRO not found at $Nro (build it with scripts\build-switch.ps1, or pass -Nro)"
}

# hbmenu folder name matches the NRO name so both titles can coexist on the card.
$appName = [System.IO.Path]::GetFileNameWithoutExtension($Nro)
$target = Join-Path $SdRoot "switch\$appName"
New-Item -ItemType Directory -Force -Path (Join-Path $target "assets") | Out-Null

Copy-Item -LiteralPath $Nro -Destination (Join-Path $target "$appName.nro") -Force
Copy-Item -Path (Join-Path $assets "*") -Destination (Join-Path $target "assets") -Recurse -Force

Write-Host "Deployed to $target :"
Get-ChildItem -Recurse -File $target | ForEach-Object {
    Write-Host "  " ($_.FullName.Substring($SdRoot.Length).TrimStart('\'))
}
Write-Host "On the console: hbmenu -> $appName -> $appName.nro"
