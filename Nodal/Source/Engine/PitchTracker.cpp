#include "PitchTracker.h"

namespace dy::nodal {

void PitchTracker::prepare (double sampleRate)
{
    decim = std::max (1, static_cast<int> (std::lround (sampleRate / 12000.0)));
    srDec = sampleRate / decim;
    reset();
}

void PitchTracker::reset()
{
    ring.fill (0.0f);
    lp = acc = 0.0f;
    accN = writePos = sinceHop = 0;
    isValid = false;
    freq = clar = 0.0f;
}

bool PitchTracker::update()
{
    if (sinceHop < kHop) return false;
    sinceHop = 0;

    // Copy the most recent kWindow + kMaxTau samples out of the ring.
    const int n = kWindow + kMaxTau;
    int start = (writePos - n) & (kRing - 1);
    double energy = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float v = ring[static_cast<size_t> ((start + i) & (kRing - 1))];
        frame[static_cast<size_t> (i)] = v;
        if (i < kWindow) energy += double (v) * v;
    }
    if (energy / kWindow < 1e-6)                        // below about -60 dBFS: nothing to track
    {
        isValid = false;
        clar = 0.0f;
        return true;
    }

    // 1-2: difference function
    d[0] = 0.0f;
    for (int tau = 1; tau <= kMaxTau; ++tau)
    {
        float s = 0.0f;
        const float* a = frame.data();
        const float* b = frame.data() + tau;
        for (int j = 0; j < kWindow; ++j) { const float x = a[j] - b[j]; s += x * x; }
        d[static_cast<size_t> (tau)] = s;
    }
    // 3: cumulative mean normalised difference
    float running = 0.0f;
    d[0] = 1.0f;
    for (int tau = 1; tau <= kMaxTau; ++tau)
    {
        running += d[static_cast<size_t> (tau)];
        d[static_cast<size_t> (tau)] = running > 0.0f ? d[static_cast<size_t> (tau)] * tau / running : 1.0f;
    }
    // 4: absolute threshold, then walk down to the local minimum
    int best = -1;
    for (int tau = kMinTau; tau < kMaxTau; ++tau)
        if (d[static_cast<size_t> (tau)] < 0.15f)
        {
            while (tau + 1 < kMaxTau && d[static_cast<size_t> (tau + 1)] < d[static_cast<size_t> (tau)]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        isValid = false;
        clar = 0.0f;
        return true;
    }
    // 5: parabolic interpolation
    double t = best;
    if (best > kMinTau && best < kMaxTau - 1)
    {
        const double y0 = d[static_cast<size_t> (best - 1)], y1 = d[static_cast<size_t> (best)], y2 = d[static_cast<size_t> (best + 1)];
        const double den = y0 - 2.0 * y1 + y2;
        if (std::abs (den) > 1e-12) t += 0.5 * (y0 - y2) / den;
    }
    freq = static_cast<float> (srDec / t);
    clar = clampT (1.0f - d[static_cast<size_t> (best)], 0.0f, 1.0f);
    isValid = true;
    return true;
}

} // namespace dy::nodal
