#pragma once
#include "NodalTypes.h"
#include <array>

namespace dy::nodal {

// YIN pitch detection (de Cheveigné & Kawahara) on a ~12 kHz decimated signal:
// 40 Hz - 1 kHz, a new estimate every ~21 ms, about 1 % of a core.
class PitchTracker
{
public:
    void prepare (double sampleRate);
    void reset();

    // Feed one input sample (mono).
    inline void push (float x)
    {
        lp += 0.5f * (x - lp);                          // gentle anti-alias before decimation
        acc += lp;
        if (++accN < decim) return;
        ring[static_cast<size_t> (writePos)] = acc / static_cast<float> (decim);
        writePos = (writePos + 1) & (kRing - 1);
        acc = 0.0f; accN = 0;
        ++sinceHop;
    }

    // Run the detector if enough new input arrived. Returns true when it did.
    bool update();

    bool  valid() const { return isValid; }
    float frequency() const { return freq; }
    float clarity() const { return clar; }

    static constexpr int kWindow = 512, kMaxTau = 320, kMinTau = 10, kHop = 256, kRing = 2048;

private:
    double srDec = 12000.0;
    int decim = 4;
    float lp = 0.0f, acc = 0.0f;
    int accN = 0, writePos = 0, sinceHop = 0;
    std::array<float, kRing> ring {};
    std::array<float, kWindow + kMaxTau> frame {};
    std::array<float, kMaxTau + 1> d {};
    bool isValid = false;
    float freq = 0.0f, clar = 0.0f;
};

} // namespace dy::nodal
