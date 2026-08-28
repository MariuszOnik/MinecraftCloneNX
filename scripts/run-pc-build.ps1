[CmdletBinding()]
param(
    [ValidateSet("configure", "build", "test", "all")]
    [string]$Action = "all",
    [ValidateSet("pc-debug", "pc-release")]
    [string]$Preset = "pc-debug",
    [string]$Commit = ""
)

$ErrorActionPreference = "Stop"

$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
$mingwCmake = "C:\devkitPro\msys2\mingw64\bin\cmake.exe"
if ($cmake -like "*\msys2\usr\bin\cmake.exe" -and (Test-Path -LiteralPath $mingwCmake)) {
    $cmake = $mingwCmake
}
$cmakeDirectory = Split-Path -Parent $cmake
$env:Path = $cmakeDirectory + [IO.Path]::PathSeparator + $env:Path
$ctest = Join-Path $cmakeDirectory "ctest.exe"
if (-not (Test-Path -LiteralPath $ctest)) {
    $ctest = (Get-Command ctest.exe -ErrorAction Stop).Source
}

function Invoke-Configure {
    $arguments = @("--preset", $Preset)
    if ($Commit) {
        $arguments += "-DVOXELGAME_BUILD_COMMIT=$Commit"
    }
    & $cmake @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Build {
    & $cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Tests {
    & $ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

switch ($Action) {
    "configure" { Invoke-Configure }
    "build" { Invoke-Build }
    "test" { Invoke-Tests }
    "all" {
        Invoke-Configure
        Invoke-Build
        Invoke-Tests
    }
}
