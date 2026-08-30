[CmdletBinding()]
param(
    # A configured build dir that contains voxelgame_assetc (any desktop preset).
    [string]$BuildDir = "build/pc-release"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

$exe = Join-Path $repo "$BuildDir/voxelgame_assetc.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    $exe = Join-Path $repo "$BuildDir/voxelgame_assetc"
}
if (-not (Test-Path -LiteralPath $exe)) {
    throw "voxelgame_assetc not found in $BuildDir -- configure and build a desktop preset first."
}

# Recompiles every assets/**/*.vxm.json and *.vxa.json into its .vxm / .vxa
# sibling. Commit the regenerated binaries alongside the JSON sources.
& $exe --dir (Join-Path $repo "assets")
exit $LASTEXITCODE
