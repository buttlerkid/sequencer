# Configure + build DY Sequencer with MSVC via Ninja (no vswhere dependency).
#   .\scripts\build.ps1            -> Release build of VST3, Standalone and engine tests
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -TestsOnly -> engine tests only (no JUCE download)
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")] [string] $Config = "Release",
    [switch] $TestsOnly,
    [string] $VsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Professional"
)
$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$vcvars = Join-Path $VsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake  = Join-Path $VsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja  = Join-Path $VsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$build  = if ($TestsOnly) { Join-Path $root "build-tests" } else { Join-Path $root "build" }
$plugin = if ($TestsOnly) { "OFF" } else { "ON" }

$cmd = "call `"$vcvars`" >nul && " +
       "`"$cmake`" -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config " +
       "-DCMAKE_MAKE_PROGRAM=`"$ninja`" -DDY_BUILD_PLUGIN=$plugin && " +
       "`"$cmake`" --build `"$build`""
cmd.exe /c $cmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

if ($TestsOnly -or (Test-Path (Join-Path $build "engine_tests.exe"))) {
    & (Join-Path $build "engine_tests.exe")
}
