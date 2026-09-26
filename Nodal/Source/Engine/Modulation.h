#pragma once
#include "NodalTypes.h"

namespace dy::nodal {

// What a modulation source can move.
enum ModTarget : int
{
    ModOff = 0, ModPitch, ModDecay, ModDamping, ModBrightness, ModStrikeX, ModStrikeY, ModSpread, ModLock, ModMix, ModDrive,
    kNumModTargets
};

inline const char* modTargetName (int t)
{
    static const char* n[kNumModTargets] = { "Off", "Body pitch", "Decay", "Damping", "Brightness", "Strike X", "Strike Y",
                                             "Spread", "Lock", "Mix", "Drive" };
    return n[clampT (t, 0, kNumModTargets - 1)];
}

enum LfoShape : int { ShapeSine = 0, ShapeTriangle, ShapeSawUp, ShapeSquare, ShapeSampleHold, ShapeDrift, kNumLfoShapes };
inline const char* lfoShapeName (int s)
{
    static const char* n[kNumLfoShapes] = { "Sine", "Triangle", "Saw", "Square", "Sample & hold", "Drift" };
    return n[clampT (s, 0, kNumLfoShapes - 1)];
}

// Tempo divisions for synced LFOs, as the cycle length in quarter notes.
struct SyncDiv { const char* name; double beats; };
inline constexpr SyncDiv kSyncDivs[] = {
    { "8 bars", 32.0 }, { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1 bar", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 },
    { "1/4 T", 2.0 / 3.0 }, { "1/8", 0.5 }, { "1/8 T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/32", 0.125 },
};
inline constexpr int kNumSyncDivs = static_cast<int> (sizeof (kSyncDivs) / sizeof (kSyncDivs[0]));

struct LfoSettings
{
    double rateHz = 0.25;
    bool   sync = false;
    int    div = 3;                  // index into kSyncDivs
    int    shape = ShapeSine;
    int    target = ModOff;
    double amount = 0.0;             // -1..1
};

struct EnvSettings
{
    double attackMs = 10.0, releaseMs = 250.0;
    int    target = ModOff;
    double amount = 0.0;
};

struct TransportInfo
{
    bool   valid = false;            // host gives a musical position
    bool   playing = false;
    double bpm = 120.0;
    double ppq = 0.0;                // at the start of the current block
};

// One LFO. Free-running in Hz, or locked to the host's song position when synced
// and playing (so it restarts identically every time the song plays).
class Lfo
{
public:
    // Advance by `samples` and return the value at the new position, -1..1.
    float advance (const LfoSettings& s, const TransportInfo& t, double ppqNow, int samples, double sr)
    {
        if (s.sync && t.valid && t.playing)
        {
            const double beats = kSyncDivs[clampT (s.div, 0, kNumSyncDivs - 1)].beats;
            phase = ppqNow / beats;
        }
        else
        {
            const double hz = s.sync ? t.bpm / 60.0 / kSyncDivs[clampT (s.div, 0, kNumSyncDivs - 1)].beats : s.rateHz;
            phase += hz * samples / sr;
        }
        const double cyc = std::floor (phase);
        cycle = static_cast<int64_t> (cyc);
        return shape (s.shape, phase - cyc);
    }

    void reset() { phase = 0.0; cycle = 0; }

    // Random values come from a hash of the cycle number, so a synced S&H or Drift
    // LFO plays back identically every time the song plays.
    static float random (int64_t c)
    {
        uint64_t z = static_cast<uint64_t> (c) + 0x9e3779b97f4a7c15ull;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        z ^= z >> 31;
        return static_cast<float> ((z >> 40) * (1.0 / 16777216.0)) * 2.0f - 1.0f;
    }

    float shape (int sh, double ph) const
    {
        switch (sh)
        {
            case ShapeSine:       return static_cast<float> (std::sin (2.0 * kPi * ph));
            case ShapeTriangle:   return static_cast<float> (1.0 - 4.0 * std::abs (ph - 0.5));
            case ShapeSawUp:      return static_cast<float> (2.0 * ph - 1.0);
            case ShapeSquare:     return ph < 0.5 ? 1.0f : -1.0f;
            case ShapeSampleHold: return random (cycle);
            case ShapeDrift:      // smooth random: cosine glide between random points
            {
                const float w = static_cast<float> (0.5 - 0.5 * std::cos (kPi * ph));
                const float a = random (cycle), b = random (cycle + 1);
                return a + (b - a) * w;
            }
            default: return 0.0f;
        }
    }

private:
    double phase = 0.0;
    int64_t cycle = 0;
};

// Peak follower with separate attack / release, output 0..1 on a -60..0 dB scale.
class EnvelopeFollower
{
public:
    void setTimes (double attackMs, double releaseMs, double sr)
    {
        att = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (0.1, attackMs) * 0.001 * sr)));
        rel = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (1.0, releaseMs) * 0.001 * sr)));
    }
    inline void push (float x)
    {
        const float a = std::abs (x);
        env += (a > env ? att : rel) * (a - env);
    }
    float level() const { return env; }
    float normalised() const
    {
        const float db = env > 1e-6f ? 20.0f * std::log10 (env) : -120.0f;
        return clampT ((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    }
    void reset() { env = 0.0f; }

private:
    float env = 0.0f, att = 0.01f, rel = 0.001f;
};

// Onsets (drum hits) from a fast and a slow envelope.
class TransientDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fast.setTimes (0.5, 25.0, sr);
        slow.setTimes (40.0, 250.0, sr);
        refractory = 0;
    }
    // Returns true on the sample where a hit is detected.
    inline bool push (float x)
    {
        fast.push (x); slow.push (x);
        if (refractory > 0) { --refractory; return false; }
        if (fast.level() > 0.02f && fast.level() > slow.level() * 2.2f + 0.004f)
        {
            refractory = static_cast<int> (sr * 0.08);
            return true;
        }
        return false;
    }
    void reset() { fast.reset(); slow.reset(); refractory = 0; }

private:
    EnvelopeFollower fast, slow;
    double sr = 48000.0;
    int refractory = 0;
};

} // namespace dy::nodal
