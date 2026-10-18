#pragma once
#include "DubTypes.h"
#include "Effects.h"
#include <atomic>

namespace dy::dub {

// Where the sequencer is at one moment: which bank, which step, and the block of
// chained steps that step belongs to.
struct StepPos
{
    int64_t absStep = 0;         // steps since the song start
    float   frac = 0.0f;         // 0..1 through that step (after swing)
    int     bank = 0;
    int     step = 0;            // 0..length-1
    int     blockStart = 0, blockLen = 1;
    float   blockPhase = 0.0f;   // 0..1 through the whole chained block
    int64_t blockAbs = 0;        // absolute step index where the block started
};

// What the UI shows, published by the audio thread.
struct EngineTelemetry
{
    std::atomic<int>   bank { 0 }, step { -1 }, blockStart { 0 }, blockLen { 1 };
    std::atomic<bool>  played { false }, running { false };
    std::atomic<float> blockPhase { 0.0f };
    std::atomic<float> stepGain { 0.0f };                        // the playing step's level right now
    std::array<std::atomic<float>, kNumTargets> modNorm {};      // modulated value, 0..1 of the target's range
    std::array<std::atomic<float>, kNumSources> source {};       // LFO 1-3 (-1..1), follower (0..1)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
};

class DubEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    void setParams (const GlobalParams& p) { params = p; }
    void setBanks (const Banks& b) { banks = b; }
    const GlobalParams& getParams() const { return params; }
    const Banks& getBanks() const { return banks; }

    // ppq: song position (quarter notes) at the first sample. running = false passes
    // the input through untouched (the delay and reverb tails still ring out).
    void process (float* L, float* R, int n, double ppq, double bpm, bool running);

    // pure function of the settings: where position `ppq` falls
    StepPos locate (double ppq) const;
    // does the block starting at absolute step `abs` play (probability)?
    bool blockPlays (const Step& s, int64_t abs, int bank) const;

    // the current value of a modulation source (-1..1, follower 0..1)
    float lfoValue (int i, double ppq) const;

    EngineTelemetry telemetry;

private:
    void control (double ppq, double bpm, int samples, bool running);

    double sr = 48000.0;
    GlobalParams params;
    Banks banks {};

    std::array<Svf, 2> filt;
    DubDelay delay;
    DubReverb reverb;

    // control-rate targets, interpolated per sample
    struct Lin { float v = 0, t = 0, d = 0; void to (float x, int n) { t = x; d = (t - v) / static_cast<float> (n); } float next() { v += d; return v; } void snap (float x) { v = t = x; d = 0; } };
    Lin gainL, gainR, dly, rev, dryMix, wetMix;
    float cutHz = 20000.0f, q = 0.5f;
    bool filterBypass = true;
    // glide: the step's values, smoothed from one step to the next
    float volS = 1.0f, cutS = 10.0f, resS = 0.0f, panS = 0.0f, fdS = 0.0f;

    std::array<double, kNumLfos> lfoPhase {};
    std::array<float, kNumLfos> lfoNow {};
    float follow = 0.0f, followAtt = 0.01f, followRel = 0.001f, followPeak = 0.0f;
    std::array<float, kNumTargets> mod {};       // plain values after modulation
    float inPk = 0, outPk = 0;
    int peakCount = 0;
};

} // namespace dy::dub
