[CmdletBinding()]
param(
    # Switch SD-card root. For Yuzu this is Settings > System > Filesystem > "SD Card".
    [Parameter(Mandatory = $true)]
    [string]$SdRoot
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot "assets"
if (-not (Test-Path -LiteralPath $source)) {
    throw "assets folder not found at $source"
}

$target = Join-Path $SdRoot "switch\voxelgame\assets"
New-Item -ItemType Directory -Force -Path $target | Out-Null

Copy-Item -Path (Join-Path $source "*") -Destination $target -Recurse -Force
Write-Host "Copied assets ->" $target
Get-ChildItem -Recurse -File $target | ForEach-Object {
    Write-Host "  " ($_.FullName.Substring($SdRoot.Length).TrimStart('\'))
}
