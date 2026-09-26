#pragma once
#include "NodalTypes.h"

namespace dy::nodal {

// Scales as cent offsets from the key, so just intonation and gamelan tunings
// sit next to the 12-TET modes.
struct ScaleDef
{
    const char* name;
    int size;
    double cents[12];
};

inline constexpr ScaleDef kScales[] = {
    { "Chromatic",        12, { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100 } },
    { "Major",             7, { 0, 200, 400, 500, 700, 900, 1100 } },
    { "Minor",             7, { 0, 200, 300, 500, 700, 800, 1000 } },
    { "Dorian",            7, { 0, 200, 300, 500, 700, 900, 1000 } },
    { "Phrygian",          7, { 0, 100, 300, 500, 700, 800, 1000 } },
    { "Lydian",            7, { 0, 200, 400, 600, 700, 900, 1100 } },
    { "Mixolydian",        7, { 0, 200, 400, 500, 700, 900, 1000 } },
    { "Harmonic minor",    7, { 0, 200, 300, 500, 700, 800, 1100 } },
    { "Major pentatonic",  5, { 0, 200, 400, 700, 900 } },
    { "Minor pentatonic",  5, { 0, 300, 500, 700, 1000 } },
    { "Blues",             6, { 0, 300, 500, 600, 700, 1000 } },
    { "Whole tone",        6, { 0, 200, 400, 600, 800, 1000 } },
    { "Just major",        7, { 0, 203.91, 386.31, 498.04, 701.96, 884.36, 1088.27 } },
    { "Slendro",           5, { 0, 240, 480, 720, 960 } },
    { "Pelog",             7, { 0, 120, 270, 540, 670, 785, 950 } },
};
inline constexpr int kNumScales = static_cast<int> (sizeof (kScales) / sizeof (kScales[0]));

inline constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Nearest note of the scale to `note` (a fractional MIDI note; the result can be
// fractional for non-12-TET scales).
inline double snapToScale (double note, int key, int scaleIdx)
{
    const auto& s = kScales[clampT (scaleIdx, 0, kNumScales - 1)];
    const double c = (note - clampT (key, 0, 11)) * 100.0;
    const double oct = std::floor (c / 1200.0);
    const double within = c - oct * 1200.0;
    double best = 0.0, bestDist = 1e9;
    for (int i = 0; i <= s.size; ++i)
    {
        const double d = i < s.size ? s.cents[i] : 1200.0;
        const double dist = std::abs (d - within);
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    return clampT (key, 0, 11) + (oct * 1200.0 + best) / 100.0;
}

// Step `degrees` scale steps from `note` (used by pitch modulation in scale mode).
inline double stepScale (double note, double degrees, int key, int scaleIdx)
{
    const auto& s = kScales[clampT (scaleIdx, 0, kNumScales - 1)];
    const double base = snapToScale (note, key, scaleIdx);
    const double c = (base - clampT (key, 0, 11)) * 100.0;
    const double oct = std::floor (c / 1200.0 + 1e-9);
    const double within = c - oct * 1200.0;
    int idx = 0;
    for (int i = 0; i < s.size; ++i) if (std::abs (s.cents[i] - within) < 0.5) idx = i;

    const double pos = idx + degrees;                       // fractional degrees glide between notes
    const double fl = std::floor (pos), frac = pos - fl;
    auto centsAt = [&] (double p)
    {
        const auto ip = static_cast<int64_t> (p);
        const int64_t o = ip >= 0 ? ip / s.size : -((-ip + s.size - 1) / s.size);
        const int d = static_cast<int> (ip - o * s.size);
        return (oct + static_cast<double> (o)) * 1200.0 + s.cents[d];
    };
    const double c0 = centsAt (fl), c1 = centsAt (fl + 1.0);
    return clampT (key, 0, 11) + (c0 + (c1 - c0) * frac) / 100.0;
}

enum TuneMode : int { TuneFree = 0, TuneScale = 1, TuneHarmonic = 2 };

// Where a mode wants to sit once locked. `free` and `f0` in Hz.
inline double lockTarget (double free, double f0, int mode, int key, int scaleIdx)
{
    if (mode == TuneScale)    return noteToHz (snapToScale (hzToNote (free), key, scaleIdx));
    if (mode == TuneHarmonic) return f0 * std::max (1.0, std::round (free / f0));
    return free;
}

// Log-domain blend between the free and the locked frequency.
inline double lockedFrequency (double free, double f0, int mode, double amount, int key, int scaleIdx)
{
    if (mode == TuneFree || amount <= 0.0) return free;
    const double target = lockTarget (free, f0, mode, key, scaleIdx);
    const double a = clampT (amount, 0.0, 1.0);
    return std::exp (std::log (free) * (1.0 - a) + std::log (target) * a);
}

} // namespace dy::nodal
