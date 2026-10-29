# Zip a built VST3 for sharing: dist\DY-<Module>-<version>-win-x64.zip
#   .\scripts\package.ps1 -Module Sequencer | Nodal
param([ValidateSet("Sequencer", "Nodal", "Pendulum", "Dub", "Mouth")] [string] $Module = "Sequencer", [string] $Config = "Release")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$product = @{ Sequencer = "DY Sequencer"; Nodal = "DY Nodal"; Pendulum = "DY Pendulum"; Dub = "DY Dub"; Mouth = "DY Mouth" }[$Module]
# target folder -> product name; Nodal ships an effect and an instrument
$products = @{ Sequencer = @(, @("DYSequencer", "DY Sequencer")); Nodal = @(@("DYNodal", "DY Nodal"), @("DYNodalInstrument", "DY Nodal Instrument")); Pendulum = @(, @("DYPendulum", "DY Pendulum")); Dub = @(, @("DYDub", "DY Dub")); Mouth = @(, @("DYMouth", "DY Mouth")) }[$Module]
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
if ($Module -eq "Mouth") {
    # the standalone too: with a virtual MIDI cable it reaches any knob in Live
    Copy-Item -Recurse (Join-Path $root "build\Mouth\DYMouth_artefacts\$Config\Standalone") (Join-Path $stage "Standalone")
}
Copy-Item (Join-Path $root "LICENSE") $stage
$readme = if ($Module -eq "Dub") {
@"
DY Dub $version  (Windows x64, VST3 audio effect)
=================================================

A step sequencer for audio. Every step has its own level, filter, pan, chance,
envelopes and delay / reverb sends, so a held chord becomes a rhythm and a loop
becomes something else.

INSTALL
  1. Copy the folder  "DY Dub.vst3"  into  C:\Program Files\Common Files\VST3\
  2. Ableton Live: Options > Preferences > Plug-Ins > "Use VST3 Plug-In System Folders" = On, Rescan.
  3. Browser > Plug-Ins > VST3 > dy > DY Dub  ->  drag onto any audio track (or after an instrument).

TRY
  Pick a preset at the top. Put a held chord or a drum loop through it and press play.
  Lanes: drag across bars to draw, click toggles, right-click a lane for tools.
  Click a step number to edit its volume / filter envelope. Right-click a knob to
  hand it to an LFO or the input follower.

Source & updates: https://github.com/buttlerkid/sequencer
"@
} elseif ($Module -eq "Mouth") {
@"
DY Mouth $version  (Windows x64, VST3 instrument, MIDI out)
===========================================================

Your webcam follows up to four faces: how open each mouth is, how wide, smiling,
puckered, the brows, the head's tilt and position. Four outputs send any of them as
a MIDI CC, pitch bend or pressure, and every open mouth plays its own note (a chord
shared out, or the pitch from how high the head is). A built-in talk-box choir is on
so you hear it at once.

INSTALL
  1. Copy the folder  "DY Mouth.vst3"  into  C:\Program Files\Common Files\VST3\
     (keep everything inside it: DYMouth.onnxruntime.dll runs the face model)
  2. Ableton Live: Options > Preferences > Plug-Ins > "Use VST3 Plug-In System Folders" = On, Rescan.
  3. Browser > Plug-Ins > VST3 > dy > DY Mouth  ->  drag onto a MIDI track. Open its window.
  4. Windows: Settings > Privacy & security > Camera > "Let desktop apps access your camera" = On.
     Close other apps that use the webcam (Zoom, Teams, OBS...).

FIRST
  Look at the camera with a relaxed, closed mouth and click "Neutral" (with friends:
  everyone at once). Open your mouth past the mark on the OPEN bar: the note plays.
  Pitch = Head height: raise and lower your head while singing.

CONTROL ANOTHER INSTRUMENT
  Turn "Preview sound" off. On a second MIDI track: MIDI From = the DY Mouth track,
  second box = "DY Mouth", Monitor = In, and put an instrument there. Wavetable and
  Operator: route Mod Wheel / Pressure in their MIDI modulation; most synths also
  take CC 74 (brightness).

CONTROL ANY KNOB IN LIVE
  Run Standalone\DY Mouth.exe next to Live. Options > Audio/MIDI Settings: MIDI
  output = a virtual MIDI cable such as loopMIDI. In Live's MIDI preferences turn
  Remote on for that port, then Ctrl+M, click a knob and move your mouth.

Face model: MediaPipe Face Landmarker (Google, Apache 2.0). Runs on your computer;
no picture ever leaves it.

Source & updates: https://github.com/buttlerkid/sequencer
"@
} elseif ($Module -eq "Pendulum") {
@"
DY Pendulum $version  (Windows x64, VST3 instrument, MIDI out)
==============================================================

A pendulum wave: each pendulum swings a little faster than the one before, so they
drift apart into waves and come back into one line at the end of every cycle.
Every swing through the middle plays a note.

INSTALL
  1. Copy the folder  "DY Pendulum.vst3"  into  C:\Program Files\Common Files\VST3\
  2. Ableton Live: Options > Preferences > Plug-Ins > "Use VST3 Plug-In System Folders" = On, Rescan.
  3. Browser > Plug-Ins > VST3 > dy > DY Pendulum  ->  drag onto a MIDI track. Press play.
     You hear its built-in bell straight away.

PLAY ANOTHER INSTRUMENT WITH IT
  Turn "Preview sound" off. On a second MIDI track: MIDI From = the DY Pendulum track,
  second box = "DY Pendulum", Monitor = In, and put any instrument on it.

Source & updates: https://github.com/buttlerkid/sequencer
"@
} elseif ($Module -eq "Nodal") {
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
