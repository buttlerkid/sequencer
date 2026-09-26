#pragma once
#include "NodalTypes.h"
#include "Bodies.h"
#include "Dsp.h"
#include "Materials.h"
#include "ModalBank.h"
#include "Modulation.h"
#include "PitchTracker.h"
#include "Tuning.h"

namespace dy::nodal {

// Everything the host can set, as plain values. The processor fills this once
// per block; the engine reads it at control rate (every kSubBlock samples).
struct EngineParams
{
    int    body = 0, material = 0;
    double pitch = 48.0;                 // body pitch as a MIDI note (the "Body size" dial)
    int    key = 0, scale = 1;
    bool   snap = true;                  // body pitch steps through the scale
    double glideMs = 80.0;
    int    tuneMode = TuneScale;
    double lock = 1.0;                   // 0..1 blend towards the locked frequencies
    int    density = 20;                 // modes in use, 1..kMaxModes
    double decay = 2.5;                  // seconds, T60 of the lowest mode
    double damping = 0.2;                // 0..1, extra high-mode damping
    double brightness = 0.0;             // -1..1
    double strikeX = 0.42, strikeY = 0.3;
    double spread = 0.6;                 // 0..1 stereo pickup spread
    double lowCut = 30.0, highCut = 18000.0;
    double driveDb = 6.0;
    double mix = 0.7;                    // 0..1
    double outDb = 0.0;

    // ---- modulation (v0.2)
    LfoSettings lfo[2];
    EnvSettings env;
    bool   trackPitch = false;           // body pitch follows the pitch heard at the input
};

class NodalEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const EngineParams& p) { params = p; }
    void setTransport (const TransportInfo& t) { transport = t; }

    // In-place stereo processing. Both pointers must be valid (pass the same buffer
    // twice for mono).
    void process (float* left, float* right, int numSamples);

    // Hit the body at the strike point with a short mallet burst (tap-to-strike in
    // the UI). Call from the audio thread; velocity 0..1.
    void strike (float velocity);

    // ---- telemetry (read after process on the audio thread, then published)
    const float* modeEnergies() const { return energies; }
    int    activeModes() const { return active; }
    int    dominantMode() const;
    double currentNote() const { return smoothNote; }
    double currentF0() const { return noteToHz (smoothNote); }
    double modeFrequency (int i) const { return freqs[clampT (i, 0, kMaxModes - 1)]; }
    void   takePeaks (float& in, float& out) { in = peakIn; out = peakOut; peakIn = peakOut = 0.0f; }
    float  lfoValue (int i) const { return lfoVals[clampT (i, 0, 1)]; }
    float  envValue() const { return envVal; }
    float  modValue (int target) const { return modVals[clampT (target, 0, kNumModTargets - 1)]; }
    double heardNote() const { return trackedNote; }         // -1 if nothing tracked yet
    float  heardClarity() const { return tracker.clarity(); }
    int    takeOnsets() { const int n = onsets; onsets = 0; return n; }

private:
    void control (int blockPos);

    EngineParams params;
    double sr = 48000.0;
    ModalBank bank;
    Svf hp, lp;
    Ramp drive, mix, outGain;
    int subPos = 0;
    double smoothNote = 48.0;
    bool first = true;

    ModeTarget targets[kMaxModes] {};
    double freqs[kMaxModes] {};
    float energies[kMaxModes] {};
    int active = 0;
    float peakIn = 0.0f, peakOut = 0.0f;

    float exc[kSubBlock] {}, wetL[kSubBlock] {}, wetR[kSubBlock] {};
    TransportInfo transport;
    Lfo lfos[2];
    EnvelopeFollower envF;
    TransientDetector transient;
    PitchTracker tracker;
    float lfoVals[2] {}, envVal = 0.0f, modVals[kNumModTargets] {};
    double trackedNote = -1.0;
    int onsets = 0;

    int   burstLeft = 0;
    float burstGain = 0.0f, burstEnv = 0.0f, burstLp = 0.0f;
    Rng   rng { 0x5eed1234u };
};

} // namespace dy::nodal
