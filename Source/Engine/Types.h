#pragma once
// Core types shared by the sequencer engine. Pure C++17, no JUCE dependency,
// so the engine can be unit-tested without building the plugin.

#include <array>
#include <cstdint>
#include <cmath>

namespace dy {

constexpr int kNumTracks = 16;
constexpr int kMaxSteps  = 64;
constexpr int kBaseNote  = 48;   // C3 = scale degree 0 at transpose 0

// Per-step editable lanes.
enum class Lane : int { Velocity = 0, Length, Timing, Probability, Repeats, Interval, Count };

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
};

struct GlobalSettings
{
    int    key          = 0;       // 0 = C
    int    scale        = 0;
    int    swingProfile = 0;
    int    swingAmount  = 0;
    double bpm          = 120.0;
    double sampleRate   = 44100.0;
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

template <typename T>
inline T clampT (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace dy
