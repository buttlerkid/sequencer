# DY Dub

A step sequencer for audio, as a VST3 effect. Every step has its own level, filter,
pan, chance, envelopes and delay / reverb sends, so a held chord turns into a
rhythm and a drum loop turns into something else.

![DY Dub](../docs/dub-v0.2.png)

## Quick start

1. Drop **DY Dub** on an audio track (or after an instrument) and press play.
2. Pick a preset at the top: *Dub Chords* on a held minor chord, *Chopped Drums* on a
   break, *Four-Bar Sweep* on a pad, *Echo Throws* on anything.
3. Turn the step knobs, click a strip to shape its envelopes on the **ENV** page.

## The console

- **Header**: preset browser (All / Factory / User, `<` `>`), **SAVE** (your own
  presets, kept in `%APPDATA%\DY\DY Dub\Presets`), **RANDOM** (a new pattern for the
  bank on view), bank keys **A / B** and the **Pattern**.
- **16 channel strips**: per step a **CUT** and **RES** knob, a **VOL** fader with a
  meter that lights while the step plays, and an **ON** key. The LED above the
  number is the playhead; click a strip to select it (white frame). The chain
  links between strips join steps; joined strips grey out, the first one plays.
- **RND CUT / RES / VOL / ON-OFF** roll one row for the bank on view;
  **COPY A > B** copies the bank.
- Pages: **ADV** (pan, chance, filter-envelope depth, delay and reverb sends, in the
  strips' columns), **MAIN** (sequencer, delay, reverb), **ENV** (the selected
  step's envelopes), **MOD** (LFOs, follower and where they go).

## The sequencer

| Control | What it does |
|---|---|
| **Bank A / B** | 16 steps each. The keys pick which bank you see; the strip gutter shows which one plays |
| **Pattern** | Which bank plays on each pass: A, B, AB, AAB, ABB, AABB, AAAB, ABBB, AAAAAAAB |
| **Seq rate** | Length of one step, 1/32 to 1 bar (1/16 T, 1/8 T, 1/4 T too) |
| **Steps** | Steps per pass (1-16); shorter than 16 runs against the bar |
| **Swing** | Delays every second step, 50 % (straight) to 75 % |
| **Glide** | Step values (level, cutoff, resonance, pan) slide into each other instead of jumping |
| **Offset** | Moves the steps up to 50 ms later or earlier against the beat |
| **Run** | *Always* keeps sequencing on its own clock while the song is stopped; *With transport* passes the input dry until you press play |

The sequencer follows the song position, so the same bar always plays the same
step, and probability decisions repeat exactly every time you play the song.

## Per step

| Control | |
|---|---|
| **ON** | Off steps leave a gap |
| **VOL** | Level of the step |
| **CUT / RES** | Filter cutoff (20 Hz - 20 kHz) and resonance; the type (LP / BP / HP) is global |
| **PAN** | Pan of the step, added to the global pan |
| **PROB** | Chance the step plays at all |
| **F.ENV** | How far the filter envelope moves the cutoff, in octaves (-5 .. +5) |
| **DLY / REV** | Send this step into the delay / the reverb: keep the pattern dry and throw one hit into the echo |
| **CHAIN** | Joins a step to the next: one longer step whose envelope runs across the whole block. The first step's values play |

Knobs and faders: drag, double-click to reset; hover shows the value. On the ADV
page drag across bars to draw, click toggles (drag to paint the same state),
double-click a bar to reset it. Right-click a lane: set every step to one value,
reset, randomise, invert, or copy the lane to the other bank.

## Envelopes

Click a strip, then open **ENV**. It shows the **volume** and **filter**
envelopes of that step (or of its chained block): rise, sustain, fall, in
percent of the step so the shape holds at any tempo.

- drag the first node sideways: rise
- drag the second node: where the fall starts, and the sustain level
- drag anywhere else up / down: bend the curve
- double-click: reset; **Copy to bank** / **Copy to both banks** give every step this shape

## Delay and reverb

Ping-pong delay locked to the tempo (1/16 to 1/2, dotted and triplet), with a high
pass and a tone low pass inside the feedback, so repeats thin out instead of piling
up. **Ping-pong** blends from a stereo echo (0 %) to repeats bouncing left - right
(100 %). Feedback at 100 % saturates instead of exploding; changing the time glides
like tape. Five reverbs: **Room, Plate, Hall, Spring, Chamber**, with size, decay,
damp, width, pre-delay and mix. Only steps with DLY / REV on feed them; the tails ring through the gaps.

**Volume** is the level of the sequenced signal, **Dry/Wet** blends it with the input,
**Output** is the final gain, **Pan** (and the strips' pan) places it.

## Modulation

Three LFOs (sine, triangle, saw up, saw down, square, sample & hold; free in Hz or
locked to the song from 1/8 to 16 bars) and an **input follower** (attack / release;
positive amount opens its target as the input gets louder, negative ducks it).

**Right-click a knob** with a modulation target (cutoff, res, env depth, delay
feedback / level, reverb size / level, pan, level, mix) and pick a source. A ring
around the knob shows where the modulation has taken it right now. Each source has
one target; set the amount on its page.

## Not yet

- modulating the per-step controls inside the grid
- per-bank step counts
