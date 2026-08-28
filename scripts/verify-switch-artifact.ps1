[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDirectory,
    [string]$ExpectedCommit = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $ArtifactDirectory).Path
$nro = Join-Path $root "voxelgame.nro"
$buildInfo = Join-Path $root "build-info.txt"
$romfsMarker = Join-Path $root "romfs\build-marker.txt"

foreach ($required in @($nro, $buildInfo, $romfsMarker)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing Switch artifact file: $required"
    }
}

$nroBytes = [System.IO.File]::ReadAllBytes($nro)
if ($nroBytes.Length -lt 20) {
    throw "NRO is too small: $($nroBytes.Length) bytes"
}

$magic = [System.Text.Encoding]::ASCII.GetString($nroBytes, 16, 4)
if ($magic -ne "NRO0") {
    throw "Invalid NRO magic at offset 0x10: '$magic'"
}

$metadata = Get-Content -Raw -LiteralPath $buildInfo
if ($ExpectedCommit -and $metadata -notmatch "(?m)^commit=$([regex]::Escape($ExpectedCommit))$") {
    throw "build-info.txt does not contain expected commit $ExpectedCommit"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $nro).Hash.ToLowerInvariant()
Write-Host "Switch artifact structure OK"
Write-Host "NRO size: $($nroBytes.Length) bytes"
Write-Host "NRO SHA-256: $hash"

