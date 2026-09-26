# Zip a built VST3 for sharing: dist\DY-<Module>-<version>-win-x64.zip
#   .\scripts\package.ps1 -Module Sequencer | Nodal
param([ValidateSet("Sequencer", "Nodal")] [string] $Module = "Sequencer", [string] $Config = "Release")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$product = @{ Sequencer = "DY Sequencer"; Nodal = "DY Nodal" }[$Module]
# target folder -> product name; Nodal ships an effect and an instrument
$products = @{ Sequencer = @(, @("DYSequencer", "DY Sequencer")); Nodal = @(@("DYNodal", "DY Nodal"), @("DYNodalInstrument", "DY Nodal Instrument")) }[$Module]
foreach ($pr in $products) {
    $src = Join-Path $root "build\$Module\$($pr[0])_artefacts\$Config\VST3\$($pr[1]).vst3"
    if (-not (Test-Path $src)) { throw "Build first: $src not found" }
}

$version = (Select-String -Path (Join-Path $root "$Module\CMakeLists.txt") -Pattern 'set\(DY_\w+_VERSION ([0-9.]+)\)').Matches[0].Groups[1].Value
$dist  = Join-Path $root "dist"
$slug  = $product -replace " ", "-"
$stage = Join-Path $dist "$slug-$version"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

foreach ($pr in $products) {
    Copy-Item -Recurse (Join-Path $root "build\$Module\$($pr[0])_artefacts\$Config\VST3\$($pr[1]).vst3") (Join-Path $stage "$($pr[1]).vst3")
}
Copy-Item (Join-Path $root "LICENSE") $stage
$readme = if ($Module -eq "Nodal") {
@"
DY Nodal $version  (Windows x64, VST3)
======================================

Two plugins:
  DY Nodal             audio effect: your audio rings a Chladni plate
  DY Nodal Instrument  instrument: MIDI notes play the plate

INSTALL
  1. Copy both folders  "DY Nodal.vst3"  and  "DY Nodal Instrument.vst3"  into
        C:\Program Files\Common Files\VST3\
     (you will be asked for administrator permission)
  2. Ableton Live: Options > Preferences > Plug-Ins
        "Use VST3 Plug-In System Folders" = On, then click Rescan.
  3. Browser > Plug-Ins > VST3 > dy:
        DY Nodal             -> drag onto any audio or instrument track (like a reverb)
        DY Nodal Instrument  -> drag onto a MIDI track and play

TRY
  Click the plate to strike it and pick a preset at the top. In the instrument,
  click the keyboard strip under the plate to play notes.
  Effect + MIDI: Play tab > Key follow or Instrument; then on a MIDI track set
  MIDI To = the DY Nodal track, second box = "DY Nodal".
  Sidechain (effect): Play tab > Sidechain > Excites, then choose the source in
  the plugin's sidechain input in Live.

Source & updates: https://github.com/buttlerkid/sequencer
"@
} else {
@"
DY Sequencer $version  (Windows x64, VST3)
==========================================

INSTALL
  1. Copy the folder  "DY Sequencer.vst3"  into
        C:\Program Files\Common Files\VST3\
     (you will be asked for administrator permission)
  2. Ableton Live: Options > Preferences > Plug-Ins
        "Use VST3 Plug-In System Folders" = On, then click Rescan.
  3. Browser > Plug-Ins > VST3 > dy > DY Sequencer  ->  drag onto a MIDI track.

HEARING IT
  The plugin outputs MIDI, not audio. On a second MIDI track set
     MIDI From = the DY Sequencer track,  second box = "DY Sequencer",
     Monitor = In,  and drop an instrument on it. Press play in Live.

QUICK START
  + Add track / click a pad      new track          Layout   drum note presets
  click cells / drag             steps              Pulses   Euclidean fills
  M / S                          mute / solo        lanes    velocity, length, timing,
  Shift                          nudge a track                probability, repeats, interval
  Export MIDI (drag it)          .mid onto a track

Source & updates: https://github.com/buttlerkid/sequencer
"@
}
$readme | Set-Content -Encoding UTF8 (Join-Path $stage "README.txt")

$zip = Join-Path $dist "$slug-$version-win-x64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Remove-Item -Recurse -Force $stage
"packaged -> $zip  ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
