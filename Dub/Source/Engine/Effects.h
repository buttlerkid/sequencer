#pragma once
#include "DubTypes.h"
#include <vector>

namespace dy::dub {

// Zavalishin / Cytomic TPT state-variable filter: stable under fast modulation.
struct Svf
{
    double g = 0, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0;
    double ic1 = 0, ic2 = 0;

    void set (double cutoff, double q, double sr)
    {
        cutoff = clampT (cutoff, 10.0, sr * 0.49);
        g = std::tan (kPi * cutoff / sr);
        k = 1.0 / std::max (0.05, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    // mode: 0 low-pass, 1 band-pass (unity peak), 2 high-pass
    inline double process (double v0, int mode)
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        if (mode == FilterLP) return v2;
        if (mode == FilterBP) return k * v1;
        return v0 - k * v1 - v2;
    }
};

// One-pole low / high pass.
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setLowpass (double hz, double sr) { a = static_cast<float> (1.0 - std::exp (-2.0 * kPi * clampT (hz, 5.0, sr * 0.45) / sr)); }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0.0f; }
};

// Smooth saturation: transparent below ~-6 dBFS, approaches +-1.
inline float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.5f) return x;
    const float s = x < 0.0f ? -1.0f : 1.0f;
    const float t = (a - 0.5f) / 0.5f;
    return s * (0.5f + 0.5f * t / (1.0f + t));
}

// Circular buffer with a fractional (linear) read.
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void resize (int n) { buf.assign (static_cast<size_t> (std::max (4, n)), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[static_cast<size_t> (w)] = x; if (++w >= static_cast<int> (buf.size())) w = 0; }
    // `d` samples behind the last written sample (d >= 1)
    inline float read (double d) const
    {
        const int n = static_cast<int> (buf.size());
        d = clampT (d, 1.0, static_cast<double> (n - 2));
        double rp = w - d;
        while (rp < 0) rp += n;
        const int i0 = static_cast<int> (rp);
        const int i1 = i0 + 1 >= n ? 0 : i0 + 1;
        const float f = static_cast<float> (rp - i0);
        return buf[static_cast<size_t> (i0)] + f * (buf[static_cast<size_t> (i1)] - buf[static_cast<size_t> (i0)]);
    }
};

// Ping-pong delay with a high pass and a tone low pass inside the feedback, so the
// repeats thin out instead of piling up. Changing the time glides like tape.
class DubDelay
{
public:
    void prepare (double sampleRate);
    void reset();
    // time in samples, feedback 0..1
    void set (double timeSamples, float feedback, float hpHz, float lpHz, bool pingPong);
    void process (float inL, float inR, float& outL, float& outR);

private:
    double sr = 48000.0;
    DelayLine l, r;
    double time = 12000.0, timeTarget = 12000.0, glide = 0.0005;
    float fb = 0.4f;
    bool pp = true;
    OnePole hpL, hpR, lpL, lpR;
};

// Five reverbs: four from one 8-line feedback delay network with different shapes,
// and a spring (dispersive allpass chains in a feedback loop: the "boing").
class DubReverb
{
public:
    void prepare (double sampleRate);
    void reset();
    // size 0..1, decay 0..1, tone 0..1 (dark .. bright)
    void set (int type, float size, float decay, float tone);
    void process (float inL, float inR, float& outL, float& outR);

private:
    static constexpr int N = 8;
    double sr = 48000.0;
    int type = RevPlate;

    // FDN
    std::array<DelayLine, N> lines;
    std::array<double, N> len {}, lenTarget {};
    std::array<float, N> gain {};
    std::array<OnePole, N> damp;
    std::array<DelayLine, 4> diff;               // input diffusers
    std::array<int, 4> diffLen {};
    float diffG = 0.6f;
    DelayLine pre;
    int preLen = 1;

    // spring
    struct Spring
    {
        DelayLine loop;
        std::array<float, 12> apZ {};
        double len = 2000.0;
        float g = 0.8f;
        OnePole lp, hp;
    };
    std::array<Spring, 2> springs;
    float springA = 0.62f;

    float lastSize = -1.0f, lastDecay = -1.0f, lastTone = -1.0f;
};

} // namespace dy::dub
