#pragma once
#include "NodalTypes.h"
#include "Bodies.h"
#include "Dsp.h"
#include "Materials.h"
#include "ModalBank.h"
#include "Modulation.h"
#include "PitchTracker.h"
#include "Tuning.h"
#include <array>

namespace dy::nodal {

// How the plugin is played (v0.4).
enum PlayMode : int { PlayEffect = 0, PlayKeyFollow, PlayInstrument, kNumPlayModes };
inline const char* playModeName (int m)
{
    static const char* n[kNumPlayModes] = { "Effect", "Key follow", "Instrument" };
    return n[clampT (m, 0, kNumPlayModes - 1)];
}

// What sets an instrument voice ringing.
enum Exciter : int { ExcMallet = 0, ExcPluck, ExcBow, ExcNoise, ExcInput, kNumExciters };
inline const char* exciterName (int e)
{
    static const char* n[kNumExciters] = { "Mallet", "Pluck", "Bow", "Noise", "Input" };
    return n[clampT (e, 0, kNumExciters - 1)];
}

// What the sidechain input does.
enum SidechainMode : int { ScOff = 0, ScExcites, ScEnvelope, kNumSidechainModes };
inline const char* sidechainModeName (int m)
{
    static const char* n[kNumSidechainModes] = { "Off", "Excites", "Envelope" };
    return n[clampT (m, 0, kNumSidechainModes - 1)];
}

inline constexpr int kMaxVoices = 8;

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

    // ---- playing (v0.4)
    int    playMode = PlayEffect;
    int    exciter = ExcMallet;
    double excTone = 0.5;                // 0..1: soft / dark to hard / bright
    double excAttackMs = 5.0, excReleaseMs = 200.0;   // sustained exciters (bow, noise, input)
    double noteDamp = 0.3;               // 0..1: how much a released key stops the ring
    double velSens = 0.7;                // 0..1
    int    polyphony = kMaxVoices;       // 1 = mono, legato with glide
    int    sidechain = ScOff;
};

class NodalEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const EngineParams& p) { params = p; }
    void setTransport (const TransportInfo& t) { transport = t; }

    // In-place stereo processing. Both pointers must be valid (pass the same buffer
    // twice for mono). The sidechain pointers may be null (not connected).
    void process (float* left, float* right, int numSamples)  { process (left, right, nullptr, nullptr, numSamples); }
    void process (float* left, float* right, const float* scLeft, const float* scRight, int numSamples);

    // Hit the body at the strike point with a short mallet burst (tap-to-strike in
    // the UI). Call from the audio thread; velocity 0..1.
    void strike (float velocity);

    // ---- MIDI (audio thread, between process calls; the processor splits blocks at events)
    void noteOn (int note, float velocity);      // velocity 0..1
    void noteOff (int note);
    void sustain (bool down);
    void pitchBend (float semitones);
    void allNotesOff();

    // ---- telemetry (read after process on the audio thread, then published)
    const float* modeEnergies() const { return energies; }
    int    activeModes() const { return active; }
    int    dominantMode() const;
    double currentNote() const { return shownNote; }
    double currentF0() const { return noteToHz (shownNote); }
    double modeFrequency (int i) const { return freqs[clampT (i, 0, kMaxModes - 1)]; }
    void   takePeaks (float& in, float& out) { in = peakIn; out = peakOut; peakIn = peakOut = 0.0f; }
    float  lfoValue (int i) const { return lfoVals[clampT (i, 0, 1)]; }
    float  envValue() const { return envVal; }
    float  modValue (int target) const { return modVals[clampT (target, 0, kNumModTargets - 1)]; }
    double heardNote() const { return trackedNote; }         // -1 if nothing tracked yet
    float  heardClarity() const { return tracker.clarity(); }
    int    takeOnsets() { const int n = onsets; onsets = 0; return n; }
    // Instrument voices: note (-1 = idle), level 0..1, key held
    int    voiceNote (int v) const   { return voices[static_cast<size_t> (clampT (v, 0, kMaxVoices - 1))].active ? voices[static_cast<size_t> (clampT (v, 0, kMaxVoices - 1))].note : -1; }
    float  voiceLevel (int v) const  { return voices[static_cast<size_t> (clampT (v, 0, kMaxVoices - 1))].level; }
    bool   voiceHeld (int v) const   { return voices[static_cast<size_t> (clampT (v, 0, kMaxVoices - 1))].gate; }
    int    activeVoices() const;

private:
    struct Voice
    {
        ModalBank bank;
        ModeTarget targets[kMaxModes] {};
        double freqs[kMaxModes] {};
        int    note = -1;
        float  velocity = 0.0f;
        bool   active = false;
        bool   gate = false;        // sounding as if the key is down (key or sustain pedal or a tap)
        bool   key = false;         // the key itself is down
        uint32_t started = 0;       // allocation order, for stealing
        double pitch = 60.0;        // current note incl. glide
        bool   fresh = true;
        int    holdSamples = 0;     // taps: keep the gate open this long
        // exciter state
        float  env = 0.0f;          // sustained exciters
        int    burst = 0;           // samples left of a mallet / pluck burst
        int    burstLen = 1;
        float  burstGain = 0.0f, burstLp = 0.0f, burstC = 0.3f, burstEnv = 0.0f, click = 0.0f;
        int    exciter = ExcMallet;
        float  gain = 1.0f;         // from velocity
        float  noiseLp = 0.0f, jitter = 0.0f, jitterTarget = 0.0f;
        int    jitterCount = 0;
        Rng    rng { 1u };
        float  level = 0.0f;        // for the UI
        int    quiet = 0;
    };

    void control (int blockPos);
    void voiceTargets (Voice& v, double f0, double releaseDamp, bool lockFundamental);
    Voice& allocate (int note);
    void trigger (Voice& v, int note, float velocity, bool legato, int excOverride = -1);
    float excite (Voice& v, float input);
    void pushHeld (int note);
    void popHeld (int note);

    EngineParams params;
    double sr = 48000.0;
    std::array<Voice, kMaxVoices> voices;
    Svf hp, lp;
    Ramp drive, mix, outGain;
    int subPos = 0;
    double smoothNote = 48.0, shownNote = 48.0;
    bool first = true;
    int lastPlayMode = -1;
    uint32_t allocCounter = 0;

    // shared per control block
    int active = 0;
    float shapeIn[kMaxModes] {}, shapeL[kMaxModes] {}, shapeR[kMaxModes] {};
    double ratioPow[kMaxModes] {};
    double ctlDecay = 2.5, ctlAlpha = 0.0, ctlTilt = 0.0, ctlLock = 1.0;
    float  ctlOutNorm = 1.0f;
    float  ctlMods[kNumModTargets] {};
    float  excAtt = 0.01f, excRel = 0.001f, noiseC = 0.3f, noiseNorm = 1.0f;

    double freqs[kMaxModes] {};
    float energies[kMaxModes] {};
    float peakIn = 0.0f, peakOut = 0.0f;

    float exc[kSubBlock] {}, vexc[kSubBlock] {}, wetL[kSubBlock] {}, wetR[kSubBlock] {};
    TransportInfo transport;
    Lfo lfos[2];
    EnvelopeFollower envF;
    TransientDetector transient;
    PitchTracker tracker;
    float lfoVals[2] {}, envVal = 0.0f, modVals[kNumModTargets] {};
    double trackedNote = -1.0;
    int onsets = 0;

    // MIDI state
    int  held[16] {};
    int  heldCount = 0;
    int  lastNote = -1;             // key follow: the last note played
    bool sustainDown = false;
    float bend = 0.0f;              // semitones

    // tap burst for the effect plate (voice 0)
    int   burstLeft = 0;
    float burstGain = 0.0f, burstEnv = 0.0f, burstLp = 0.0f;
    Rng   rng { 0x5eed1234u };
};

} // namespace dy::nodal
