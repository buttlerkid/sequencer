#pragma once
#include "NodalTypes.h"

namespace dy::nodal {

// Zavalishin / Cytomic TPT state-variable filter: stable under fast modulation.
struct Svf
{
    double g = 0, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0;
    double ic1 = 0, ic2 = 0;

    void set (double cutoff, double q, double sr)
    {
        cutoff = clampT (cutoff, 5.0, sr * 0.49);
        g = std::tan (kPi * cutoff / sr);
        k = 1.0 / std::max (0.05, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    // Returns low-pass; high-pass through the reference.
    inline double process (double v0, double& high)
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        high = v0 - k * v1 - v2;
        return v2;
    }
};

// Linear ramp towards a target over a fixed number of samples (no zipper noise).
struct Ramp
{
    float value = 0, target = 0, step = 0;
    int left = 0;
    void snap (float v) { value = target = v; step = 0; left = 0; }
    void set (float v, int samples)
    {
        if (v == target) return;
        target = v;
        left = std::max (1, samples);
        step = (target - value) / static_cast<float> (left);
    }
    inline float next()
    {
        if (left > 0) { value += step; if (--left == 0) value = target; }
        return value;
    }
};

// Smooth saturation: transparent below ~-6 dBFS, approaches +-1.
inline float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.5f) return x;
    const float s = x < 0.0f ? -1.0f : 1.0f;
    const float t = (a - 0.5f) / 0.5f;
    return s * (0.5f + 0.5f * t / (1.0f + t));      // 0.5 + 0.5 * (t / (1 + t)), continuous slope 1 at 0.5
}

} // namespace dy::nodal
