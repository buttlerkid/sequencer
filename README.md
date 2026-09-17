# DY Sequencer

A 16-track Euclidean / scale-aware MIDI step sequencer as a VST3 (and standalone
app), built with JUCE. Designed for Ableton Live but works in any VST3 host.

![v0.2 dark theme](docs/screenshot-v0.2-dark.png)

## Features

**Tracks**
- Up to **16 tracks**, added one at a time (**+ Add track** or click an empty pad),
  each with its own length (1–64 steps), rate (1/4 … 1/64 incl. triplets), MIDI
  channel, name, **Mute**, **Solo** and timing **Shift** (±64 ms). Different
  lengths cycle against each other as polymeters.
- **All tracks at once**: every track is a row in the overview grid — name, note,
  shift, M/S, Steps / Pulses / Rotate, then its step cells. Click a cell to
  toggle, drag to paint. Scrolls past 10 tracks.
- **Euclidean sequencing** per track: Pulses + Rotate, in three modes — *Off*
  (manual only), *Add* (manual ∪ Euclid), *Only* (Euclid replaces manual).

**Pitch**
- **Scale mode**: global Key (12) and Scale (14), per-track transpose in scale
  degrees, per-step **Interval** lane in degrees. **Fixed mode**: one MIDI note
  per track (drums).
- **Pads**: 4×4, one per track, showing note + name and lighting up on every hit.
  Click a pad to select + audition its note (works with the transport stopped).
  **Layout** presets assign all 16 notes at once: GM Drums, Chromatic from C1
  (Ableton Drum Rack), or Melodic.

**Per step** (six lanes): Velocity, Length (5–200 %), Timing (±64 ms),
Probability, Repeats (1–8 ratchets), Interval (±24 degrees). Drag to draw, sweep
horizontally to paint, double-click to reset.

**Lane macros** per track — knobs that offset a whole lane on top of the step
values: Velocity ±64, Length 25–400 %, Shift ±64 ms, Prob 0–100 %, Reps +0–7,
Interval (transpose). All host-automatable.

**Groove**: 8 shuffle profiles (Classic, Shuffle, Lazy, Push, Lean, Drunk, Roll,
Half-Time), global amount, per-track *Global / Off / Custom*, and a **Master
shift** (±64 ms) for the whole sequencer.

**Generators**: random steps, random notes, arpeggio up / down / up-down /
random, per-lane randomise and reset. **Clear all** with confirmation.

**Workflow**: copy / paste a track or the whole pattern (destination keeps its
channel / on / mute / solo); **Export MIDI** — drag the button onto a DAW track
to drop a multi-track `.mid` (1–16 bars), or click it to save a file. Resizable
50 %–400 %, dark and light themes, tooltips everywhere. 325 automatable
parameters; per-step data, names and UI preferences are saved with the plugin
state.

**Timing**: sample-accurate PPQ transport sync, loop-point aware, deterministic
across block sizes. The standalone app free-runs at 120 BPM.

## Installing (Windows, Ableton Live)

1. Download `DY-Sequencer-<version>-win-x64.zip` from the
   [Releases](https://github.com/buttlerkid/sequencer/releases) page (or build it,
   see below).
2. Unzip and copy the **`DY Sequencer.vst3`** folder into
   `C:\Program Files\Common Files\VST3\`.
3. In Live: **Options → Preferences → Plug-Ins**, make sure **Use VST3 Plug-In
   System Folders** is **On**, then **Rescan**.
4. In the browser: **Plug-Ins → VST3 → dy → DY Sequencer**. Drag it onto a MIDI
   track.

Requirements: Windows 10/11 x64, Live 10.1+ (any VST3 host). No runtime
installers needed — the plugin is statically linked.

### Hearing it

The plugin makes MIDI, not sound. On a second MIDI track:

- **MIDI From** → the DY Sequencer track, second dropdown → **DY Sequencer**
- **Monitor** → **In**
- drop any instrument (Operator, Drum Rack, …)
- press **play** in Live

For 16 separate instruments, repeat per receiver track; each receiver gets all
channels, so give each DY track its own channel and filter by channel on the
receiving side (Drum Rack / Instrument Rack chains, or a channel-filter device).

## Building

Visual Studio 2022 with the C++ workload (its bundled CMake + Ninja are used).
First configure downloads JUCE 9.0.2 into `build/_deps`.

```powershell
.\scripts\build.ps1              # Release: VST3 + Standalone + engine tests
.\scripts\build.ps1 -TestsOnly   # engine tests only (no JUCE)
.\scripts\install.ps1            # copy the VST3 into Common Files\VST3 (asks for admin)
.\scripts\package.ps1            # zip for sharing -> dist\DY-Sequencer-<version>-win-x64.zip
```

The engine (`Source/Engine`) has no JUCE dependency and is covered by
`Tests/EngineTests.cpp` (1,765 checks: Euclid, scales, swing, lanes, macros,
timing, transport jumps, block-size independence, offline render).

## Project layout

```
Source/
  Engine/        pure C++17 core (no JUCE): Euclid, Scale, Shuffle, Pattern
                 (lock-free step data), Sequencer (PPQ clock -> events), Generator
  Params/        AudioProcessorValueTreeState layout + cached raw pointers
  UI/            Theme/LookAndFeel, HeaderBar, OverviewGrid (all tracks),
                 PadsPanel, EditPanel (settings, lanes, macros, generators),
                 GlobalPanel, LaneEditor, StatusBar
  PluginProcessor.*   transport, solo resolution, audition queue, state,
                      clipboard, MIDI export
  PluginEditor.*      fixed logical layout (1200x740) scaled to the window
Tests/EngineTests.cpp
scripts/        build.ps1, install.ps1, package.ps1, shot.ps1, click.ps1, lclick.ps1
```

### How timing works

Each track keeps a cursor over an absolute step timeline `k · division` (PPQ).
Per block the engine schedules every step whose nominal time falls before
`blockEnd + maxOffset` (swing + up to 192 ms of step / track / master shift),
computes its real time and emits it once that time lands inside a block. Steps
already in the past after a transport jump are dropped rather than played late.
Note-offs are tracked in PPQ; stopping the transport releases everything.

## Roadmap ideas

- Pattern banks per track with quantised switching
- Incoming MIDI → live transposition / scale root, chord-follow
- Per-step conditions (1:2, 1:4, fill), humanise
- Push / Launchpad grid control
- macOS / AU build (the CMake project is already cross-platform)

## License

Project code: MIT. Built on [JUCE](https://juce.com) under AGPLv3 — distributing
closed-source binaries requires a JUCE commercial licence.
