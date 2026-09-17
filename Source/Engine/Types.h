#pragma once
// Core types shared by the sequencer engine. Pure C++17, no JUCE dependency,
// so the engine can be unit-tested without building the plugin.

#include <array>
#include <cstdint>
#include <cmath>

namespace dy {

template <typename T>
inline T clampT (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

constexpr int kNumTracks   = 16;
constexpr int kMaxSteps    = 64;
constexpr int kNumPatterns = 8;    // pattern banks A..H
constexpr int kBaseNote  = 48;   // C3 = scale degree 0 at transpose 0

// Per-step editable lanes.
enum class Lane : int { Velocity = 0, Length, Timing, Probability, Repeats, Interval, Condition, Count };

// Trig conditions (Lane::Condition values).
enum Condition : int
{
    CondAlways = 0,
    Cond1of2, Cond2of2, Cond1of3, Cond2of3, Cond3of3, Cond1of4, Cond2of4, Cond3of4, Cond4of4, Cond1of8, Cond8of8,
    CondFill, CondNotFill, CondFirst, CondPrev, CondNotPrev,
    kNumConditions
};

inline const char* conditionName (int c)
{
    static const char* names[kNumConditions] = {
        "-", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "1:8", "8:8",
        "Fill", "!Fill", "1st", "Prev", "!Prev"
    };
    return names[clampT (c, 0, kNumConditions - 1)];
}

// A:B conditions as (a, b); (0, 0) for the others.
inline void conditionRatio (int c, int& a, int& b)
{
    static const int table[kNumConditions][2] = {
        { 0, 0 }, { 1, 2 }, { 2, 2 }, { 1, 3 }, { 2, 3 }, { 3, 3 }, { 1, 4 }, { 2, 4 }, { 3, 4 }, { 4, 4 }, { 1, 8 }, { 8, 8 },
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
    };
    a = table[clampT (c, 0, kNumConditions - 1)][0];
    b = table[clampT (c, 0, kNumConditions - 1)][1];
}

struct LaneInfo
{
    const char* name;
    int min, max, def;
    bool bipolar;      // drawn from a centre line
    const char* unit;
};

inline const LaneInfo& laneInfo (Lane l)
{
    static const LaneInfo table[] = {
        { "Velocity",    1,   127, 100, false, ""    },
        { "Length",      5,   200,  80, false, "%"   },
        { "Timing",    -64,    64,   0, true,  "ms"  },
        { "Probability", 0,   100, 100, false, "%"   },
        { "Repeats",     1,     8,   1, false, "x"   },
        { "Interval",  -24,    24,   0, true,  "deg" },
        { "Condition",   0, kNumConditions - 1, 0, false, "" },
    };
    return table[static_cast<int> (l)];
}

enum EuclidMode : int { EuclidOff = 0, EuclidAdd = 1, EuclidOnly = 2 };
enum PitchMode  : int { PitchScale = 0, PitchFixed = 1 };
enum SwingMode  : int { SwingGlobal = 0, SwingOff = 1, SwingCustom = 2 };

// Snapshot of one track's host-automatable parameters for a single block.
struct TrackSettings
{
    bool   enabled      = false;
    bool   mute         = false;
    int    channel      = 1;       // 1..16
    int    steps        = 16;      // 1..64
    double division     = 0.25;    // PPQ per step (0.25 = 1/16)
    int    pulses       = 0;       // Euclid hits, 0 = off
    int    rotate       = 0;
    int    euclidMode   = EuclidAdd;
    int    pitchMode    = PitchScale;
    int    fixedNote    = 36;
    int    transpose    = 0;       // scale degrees
    int    swingMode    = SwingGlobal;
    int    swingProfile = 0;
    int    swingAmount  = 50;      // 0..100
    bool   solo         = false;   // resolved into `mute` by the host layer
    double shiftMs      = 0.0;     // whole-track timing offset
    // Lane macros: applied on top of every step's own value.
    int    velOffset    = 0;       // -64..64
    int    lengthScale  = 100;     // 25..400 %
    int    probScale    = 100;     // 0..100 %
    int    repsAdd      = 0;       // 0..7
};

struct GlobalSettings
{
    int    key          = 0;       // 0 = C
    int    scale        = 0;
    int    swingProfile = 0;
    int    swingAmount  = 0;
    double masterShiftMs = 0.0;    // global timing offset
    bool   fill         = false;   // Fill / !Fill conditions
    int    humanizeTimeMs = 0;     // +/- random timing per hit
    int    humanizeVel  = 0;       // +/- random velocity per hit
    int    midiTranspose = 0;      // semitones added to scale-mode notes (MIDI key follow)
    double bpm          = 120.0;
    double sampleRate   = 44100.0;
};

// A pending pattern change: steps whose nominal time is >= atPpq come from `next`.
struct PatternSwitch
{
    const struct PatternModel* next = nullptr;
    double atPpq = 0.0;
};

struct Transport
{
    bool   playing    = false;
    double ppqStart   = 0.0;
    int    numSamples = 0;
};

struct MidiEvent
{
    double ppq;           // absolute position in quarter notes
    bool   noteOn;
    int    channel;       // 1..16
    int    note;          // 0..127
    int    velocity;      // 1..127 (0 for note-off)
    int    track = 0;     // originating sequencer track
    int    sampleOffset = 0;
};

// floor-division helpers that behave for negative numbers
inline int64_t floorDiv (int64_t a, int64_t b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
inline int     posMod   (int64_t a, int b)     { int m = static_cast<int> (a % b); return m < 0 ? m + b : m; }

} // namespace dy
