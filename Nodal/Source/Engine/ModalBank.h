#pragma once
#include "NodalTypes.h"

namespace dy::nodal {

struct ModeTarget
{
    double freq = 440.0;   // Hz
    double t60  = 1.0;     // seconds to decay by 60 dB
    float  in   = 0.0f;    // excitation weight (mode shape at the strike point, tilt)
    float  outL = 0.0f;    // pickup weights
    float  outR = 0.0f;
};

// A bank of modal resonators. Each mode is a complex one-pole z = r * e^(jw):
// it rotates at the mode frequency and shrinks by r per sample, which is exactly
// a decaying sinusoid. Advantages over biquads: unconditionally stable, amplitude
// continuous when the frequency moves, and |state| is the mode's amplitude (the
// visualiser reads it directly).
//
// Excitation is scaled by sqrt(1 - r^2) so broadband input produces the same
// energy per mode whatever the decay; the magnitude of each state is clamped so a
// tone sitting exactly on a long-decay mode cannot run away (the clamp scales the
// complex state, so it stays a pure sinusoid - no distortion).
class ModalBank
{
public:
    void prepare (double sampleRate);
    void reset();

    // Targets for the next kSubBlock samples; coefficients and gains ramp linearly
    // across that period. Must be called exactly every kSubBlock samples.
    void setTargets (const ModeTarget* targets, int count);

    // Advance `n` samples (n <= samples left in the current sub-block), adding the
    // output into outL / outR.
    void process (const float* in, float* outL, float* outR, int n);

    float amplitude2 (int i) const { return re[i] * re[i] + im[i] * im[i]; }
    int   size() const { return count; }

    float limit = 1.0f;

private:
    double sr = 48000.0;
    int count = 0;
    bool fresh = true;

    float re[kMaxModes] {}, im[kMaxModes] {};
    float cr[kMaxModes] {}, ci[kMaxModes] {}, dcr[kMaxModes] {}, dci[kMaxModes] {};
    float gi[kMaxModes] {}, dgi[kMaxModes] {};
    float gl[kMaxModes] {}, dgl[kMaxModes] {};
    float gr[kMaxModes] {}, dgr[kMaxModes] {};
};

} // namespace dy::nodal
