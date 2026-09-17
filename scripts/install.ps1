# Copy the built VST3 into a folder Ableton Live scans.
#   .\scripts\install.ps1          -> C:\Program Files\Common Files\VST3 (elevates if needed)
#   .\scripts\install.ps1 -User    -> %LOCALAPPDATA%\VST3 (no admin; set it as Live's custom VST3 folder)
param([switch] $User, [string] $Config = "Release")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $root "build\DYSequencer_artefacts\$Config\VST3\DY Sequencer.vst3"
if (-not (Test-Path $src)) { throw "Build first: $src not found" }

$dest = if ($User) { Join-Path $env:LOCALAPPDATA "VST3" } else { Join-Path $env:CommonProgramFiles "VST3" }

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
             [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $User -and -not $isAdmin) {
    Write-Host "Elevating to copy into $dest ..."
    $args = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Config $Config"
    Start-Process powershell -Verb RunAs -ArgumentList $args -Wait
    exit
}

New-Item -ItemType Directory -Force $dest | Out-Null
$target = Join-Path $dest "DY Sequencer.vst3"
if (Test-Path $target) { Remove-Item -Recurse -Force $target }
Copy-Item -Recurse $src $target
Write-Host "Installed -> $target"
if ($User) {
    Write-Host "In Live: Options > Preferences > Plug-Ins > 'Use VST3 Plug-In Custom Folder' = $dest, then Rescan."
}
