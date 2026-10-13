#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dy::pendulum {

constexpr int kMaxPendulums = 24;
constexpr double kPi = 3.14159265358979323846;

// When a pendulum makes a note.
enum Trigger : int { TrigCrossings = 0, TrigRightward, TrigLeftward, TrigTurns, kNumTriggers };
inline const char* triggerName (int t)
{
    static const char* n[kNumTriggers] = { "Crossings", "Rightward", "Leftward", "Turns" };
    return n[t < 0 ? 0 : t >= kNumTriggers ? kNumTriggers - 1 : t];
}

// Quantize grid, in quarter notes (0 = off: exact physical timing).
struct Grid { const char* name; double beats; };
inline constexpr Grid kGrids[] = { { "Off", 0.0 }, { "1/32", 0.125 }, { "1/16 T", 1.0 / 6.0 }, { "1/16", 0.25 },
                                   { "1/8 T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/4", 1.0 } };
inline constexpr int kNumGrids = static_cast<int> (sizeof (kGrids) / sizeof (kGrids[0]));

// Note names come from scales given in semitones.
struct Scale { const char* name; int size; int steps[12]; };
inline constexpr Scale kScales[] = {
    { "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "Harmonic minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Major pentatonic", 5, { 0, 2, 4, 7, 9 } },
    { "Minor pentatonic", 5, { 0, 3, 5, 7, 10 } },
    { "Blues", 6, { 0, 3, 5, 6, 7, 10 } },
    { "Whole tone", 6, { 0, 2, 4, 6, 8, 10 } },
    { "Hirajoshi", 5, { 0, 2, 3, 7, 8 } },
    { "Slendro (approx.)", 5, { 0, 2, 5, 7, 10 } },
};
inline constexpr int kNumScales = static_cast<int> (sizeof (kScales) / sizeof (kScales[0]));

struct PendulumParams
{
    int    count = 12;               // pendulums, 1..kMaxPendulums
    double cycleBeats = 32.0;        // quarter notes until they all line up again
    int    swings = 16;              // full swings the longest pendulum makes per cycle
    int    trigger = TrigCrossings;
    int    quantize = 0;             // index into kGrids
    int    root = 48;                // MIDI note of the longest pendulum
    int    scale = 9;                // index into kScales
    int    step = 1;                 // scale degrees between neighbouring pendulums
    bool   descending = false;       // true: longest pendulum gets the highest note
    double noteBeats = 0.25;         // note length
    float  velocity = 0.8f;          // 0..1
    float  accent = 0.5f;            // 0..1: how much being in phase with the others adds
    int    channel = 1;              // 1..16
};

struct NoteEvent
{
    int   sample;                    // offset in the block
    int   note;
    float velocity;                  // 0 for note off
    bool  on;
    int   pendulum;
    int   channel;
};

// Pendulum k makes (swings + k) full swings per cycle, so each one swings a little
// faster than the one before: they drift apart into waves and come back into one
// line at the end of every cycle (Gamma = cycleBeats). Everything is a function of
// the song position, so a given bar always sounds the same, whatever the buffer size.
class PendulumEngine
{
public:
    void prepare (double sampleRate) { sr = sampleRate > 0 ? sampleRate : 48000.0; reset(); }
    void reset();
    void setParams (const PendulumParams& p);
    const PendulumParams& getParams() const { return params; }

    // Generate the notes for [ppq, ppq + numSamples) at `bpm`; appends to `out`, sorted.
    void process (double ppq, double bpm, int numSamples, std::vector<NoteEvent>& out);
    // Release everything that is sounding (transport stop, jump, parameter change).
    void allNotesOff (int sample, std::vector<NoteEvent>& out);

    // ---- for the display
    int    count() const { return params.count; }
    double swingsOf (int k) const { return params.swings + k; }
    double position (int k, double ppq) const;               // -1..1, +1 = released side
    double coherence (double ppq) const;                     // 0..1: 1 = all in one line
    int    noteOf (int k) const;
    double lastHit (int k) const { return hits[static_cast<size_t> (clampIndex (k))]; }
    // when pendulum k last made (or would have made) a note, at or before ppq
    double lastEventBefore (int k, double ppq) const;

private:
    static int clampIndex (int k) { return k < 0 ? 0 : k >= kMaxPendulums ? kMaxPendulums - 1 : k; }
    void emitOffsets (double p0, double p1, int n, std::vector<NoteEvent>& out);
    void releaseUntil (double p0, double p1, double limit, int n, std::vector<NoteEvent>& out);

    PendulumParams params;
    double sr = 48000.0;
    struct Sounding { int note, channel; double offPpq; };
    std::vector<Sounding> sounding;
    std::array<double, kMaxPendulums> lastSlot {};
    std::array<double, kMaxPendulums> hits {};
};

} // namespace dy::pendulum
