#pragma once
// Core types for the DY Dub engine. Pure C++17, no JUCE, so the engine can be
// unit-tested without building the plugin.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace dy::dub {

constexpr double kPi      = 3.14159265358979323846;
constexpr int    kSteps   = 16;      // steps per bank
constexpr int    kBanks   = 2;       // A and B
constexpr int    kSubBlock = 16;     // control-rate period in samples

template <typename T>
inline T clampT (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

// Rise / sustain / fall over one step (or one chained block), in fractions of its length,
// so the shape holds at any tempo. 0 -> 1 over `rise`, 1 -> `sustain` through the middle,
// `sustain` -> 0 over the last `fall`. `curve` bends every segment (-1 fast .. +1 slow start).
struct Env
{
    float rise = 0.01f, fall = 0.10f, sustain = 1.0f, curve = 0.0f;

    static float bend (float t, float c)
    {
        t = clampT (t, 0.0f, 1.0f);
        if (c >= 0.0f) return std::pow (t, 1.0f + 3.0f * c);
        return 1.0f - std::pow (1.0f - t, 1.0f - 3.0f * c);
    }

    float at (float ph) const
    {
        ph = clampT (ph, 0.0f, 1.0f);
        float r = clampT (rise, 0.0f, 1.0f), f = clampT (fall, 0.0f, 1.0f);
        if (r + f > 1.0f) { const float k = 1.0f / (r + f); r *= k; f *= k; }
        const float s = clampT (sustain, 0.0f, 1.0f);
        if (ph < r) return bend (ph / r, curve);
        if (f > 0.0f && ph > 1.0f - f) return s * bend ((1.0f - ph) / f, curve);
        const float mid = 1.0f - r - f;
        if (mid <= 1e-6f) return s;
        return 1.0f + (s - 1.0f) * bend ((ph - r) / mid, -curve);
    }
};

struct Step
{
    bool  on = true;
    float vol = 1.0f;            // 0..1
    float cut = 1.0f;            // 0..1 -> 20 Hz .. 20 kHz on an octave scale
    float res = 0.0f;            // 0..1
    float pan = 0.0f;            // -1..1
    float prob = 1.0f;           // 0..1
    float fdepth = 0.0f;         // filter envelope depth, octaves -5..5
    bool  chain = false;         // joins this step to the next one
    bool  dly = false, rev = false;
    Env   venv, fenv { 0.0f, 0.0f, 0.0f, 0.3f };

    bool operator== (const Step& o) const
    {
        auto e = [] (const Env& a, const Env& b) { return a.rise == b.rise && a.fall == b.fall && a.sustain == b.sustain && a.curve == b.curve; };
        return on == o.on && vol == o.vol && cut == o.cut && res == o.res && pan == o.pan && prob == o.prob && fdepth == o.fdepth
            && chain == o.chain && dly == o.dly && rev == o.rev && e (venv, o.venv) && e (fenv, o.fenv);
    }
};

using Bank = std::array<Step, kSteps>;
using Banks = std::array<Bank, kBanks>;

inline double cutToHz (double oct) { return 20.0 * std::pow (2.0, oct); }      // cut * 10 octaves above 20 Hz

// ------------------------------------------------------------------ choices
struct RateDef { const char* name; double beats; };
inline constexpr RateDef kRates[] = {
    { "1/32", 0.125 }, { "1/16 T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/8 T", 1.0 / 3.0 }, { "1/8", 0.5 },
    { "1/4 T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 bar", 4.0 },
};
inline constexpr int kNumRates = static_cast<int> (sizeof (kRates) / sizeof (kRates[0]));

// How the two banks follow each other, one letter per pass through the steps.
inline constexpr const char* kPatterns[] = { "A", "B", "AB", "AAB", "ABB", "AABB", "AAAB", "ABBB", "AAAAAAAB" };
inline constexpr int kNumPatterns = static_cast<int> (sizeof (kPatterns) / sizeof (kPatterns[0]));

inline constexpr RateDef kDelayTimes[] = {
    { "1/16", 0.25 }, { "1/8 T", 1.0 / 3.0 }, { "1/16 D", 0.375 }, { "1/8", 0.5 }, { "1/4 T", 2.0 / 3.0 },
    { "1/8 D", 0.75 }, { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/2", 2.0 },
};
inline constexpr int kNumDelayTimes = static_cast<int> (sizeof (kDelayTimes) / sizeof (kDelayTimes[0]));

inline constexpr RateDef kLfoDivs[] = {
    { "1/8", 0.5 }, { "1/4 T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 bar", 4.0 }, { "2 bars", 8.0 },
    { "4 bars", 16.0 }, { "8 bars", 32.0 }, { "16 bars", 64.0 },
};
inline constexpr int kNumLfoDivs = static_cast<int> (sizeof (kLfoDivs) / sizeof (kLfoDivs[0]));

enum FilterMode : int { FilterLP = 0, FilterBP, FilterHP };
enum ReverbType : int { RevRoom = 0, RevPlate, RevHall, RevSpring, RevChamber, kNumReverbTypes };
inline const char* reverbName (int t)
{
    static const char* n[kNumReverbTypes] = { "Room", "Plate", "Hall", "Spring", "Chamber" };
    return n[clampT (t, 0, kNumReverbTypes - 1)];
}

enum LfoShape : int { ShapeSine = 0, ShapeTriangle, ShapeSawUp, ShapeSawDown, ShapeSquare, ShapeSampleHold, kNumShapes };
inline const char* shapeName (int s)
{
    static const char* n[kNumShapes] = { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Sample & hold" };
    return n[clampT (s, 0, kNumShapes - 1)];
}

// ------------------------------------------------------------------ modulation targets
// Each target is a global control with a linear range; sources add `amount * value`
// in that range's 0..1 space, so a ring on the knob shows exactly where it is.
enum ModTarget : int
{
    ModOff = 0, ModCutoff, ModRes, ModLevel, ModPan, ModEnvDepth, ModDelayFb, ModDelayLevel, ModRevSize, ModRevLevel, ModMix,
    kNumTargets
};

struct TargetDef { const char* name; float lo, hi; };
inline const TargetDef& targetDef (int t)
{
    static const TargetDef d[kNumTargets] = {
        { "Off", 0, 1 }, { "Cutoff", -4, 4 }, { "Resonance", -1, 1 }, { "Level", 0, 1.5f }, { "Pan", -1, 1 }, { "Env depth", 0, 2 },
        { "Delay feedback", 0, 1 }, { "Delay level", 0, 1 }, { "Reverb size", 0, 1 }, { "Reverb level", 0, 1 }, { "Mix", 0, 1 },
    };
    return d[clampT (t, 0, kNumTargets - 1)];
}

constexpr int kNumLfos = 3;
constexpr int kNumSources = kNumLfos + 1;          // + the envelope follower

struct LfoSettings
{
    double rateHz = 1.0;
    bool   sync = true;
    int    div = 4;                // index into kLfoDivs
    int    shape = ShapeSine;
    int    target = ModOff;
    float  amount = 0.0f;          // -1..1
};

struct FollowerSettings
{
    double attackMs = 10.0, releaseMs = 200.0;
    int    target = ModOff;
    float  amount = 0.0f;          // -1..1: open the target as the input gets louder, or duck it
};

struct GlobalParams
{
    bool   runAlways = true;       // run on our own clock while the host is stopped
    int    rate = 2;               // index into kRates
    float  swing = 0.0f;           // 0..0.5
    float  glide = 0.0f;           // 0..1: step values slide into each other (fraction of a step)
    float  offsetMs = 0.0f;        // -50..50: the steps land this much later (or earlier)
    int    length = kSteps;        // steps per pass, 1..16
    int    pattern = 0;            // index into kPatterns
    int    filterMode = FilterLP;

    // the modulatable controls, indexed by ModTarget (plain values in targetDef ranges)
    std::array<float, kNumTargets> target { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.45f, 0.5f, 0.5f, 0.3f, 1.0f };

    int    delayTime = 3;          // index into kDelayTimes
    float  delayHpHz = 300.0f, delayLpHz = 6000.0f;
    float  pingPong = 1.0f;        // 0 stereo echo .. 1 full ping-pong

    int    revType = RevPlate;
    float  revDecay = 0.5f, revTone = 0.5f;
    float  revWidth = 1.0f;        // 0 mono .. 1 full stereo
    float  revPreMs = 0.0f;        // added to the type's own pre-delay

    float  outputDb = 0.0f;

    std::array<LfoSettings, kNumLfos> lfo {};
    FollowerSettings follower {};
};

// Cheap stateless hash for probability and sample & hold: the same song position
// always gives the same result.
inline float hash01 (int64_t n, uint64_t salt = 0)
{
    uint64_t z = static_cast<uint64_t> (n) * 0x9e3779b97f4a7c15ull + salt * 0xd1b54a32d192ed03ull + 0x632be59bd9b4e019ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    z ^= z >> 31;
    return static_cast<float> ((z >> 40) * (1.0 / 16777216.0));
}

} // namespace dy::dub
