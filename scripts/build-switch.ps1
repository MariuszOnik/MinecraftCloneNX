[CmdletBinding()]
param(
    [string]$Commit = ""
)

$ErrorActionPreference = "Stop"
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
$devkitProCmake = "C:\devkitPro\msys2\usr\bin\cmake.exe"
if (Test-Path -LiteralPath $devkitProCmake) {
    $cmake = $devkitProCmake
}
$arguments = @("--preset", "switch-release")
if ($Commit) {
    $arguments += "-DVOXELGAME_BUILD_COMMIT=$Commit"
}

& $cmake @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build --preset switch-release
exit $LASTEXITCODE
