#include "ModalBank.h"

namespace dy::nodal {

void ModalBank::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    reset();
}

void ModalBank::reset()
{
    for (int i = 0; i < kMaxModes; ++i)
    {
        re[i] = im[i] = 0.0f;
        dcr[i] = dci[i] = dgi[i] = dgl[i] = dgr[i] = 0.0f;
    }
    fresh = true;
}

void ModalBank::setTargets (const ModeTarget* t, int n)
{
    n = clampT (n, 0, kMaxModes);
    const float inv = 1.0f / static_cast<float> (kSubBlock);

    for (int i = 0; i < n; ++i)
    {
        const double f   = clampT (t[i].freq, 1.0, sr * 0.49);
        const bool audible = t[i].freq < sr * 0.45;
        const double r   = std::exp (-6.907755 / (clampT (t[i].t60, 0.005, 120.0) * sr));
        const double w   = 2.0 * kPi * f / sr;
        const float tcr  = static_cast<float> (r * std::cos (w));
        const float tci  = static_cast<float> (r * std::sin (w));
        // sqrt(1 - r^2) alone keeps broadband power per mode constant, but then a drum hit
        // on a long decay barely rings. Leaning 30 % towards impulse-preserving (and
        // measuring the pole distance at 48 kHz so it does not depend on the rate)
        // lets hits ring out while noise stays within a few dB across decays.
        const double q = std::max (1e-12, 1.0 - r * r);
        const float norm = static_cast<float> (std::sqrt (q) * std::pow (std::max (1e-12, q * sr / 48000.0), -0.15));
        const float tgi  = audible ? t[i].in * norm : 0.0f;
        const float tgl  = audible ? t[i].outL : 0.0f;
        const float tgr  = audible ? t[i].outR : 0.0f;

        if (fresh || i >= count)
        {
            cr[i] = tcr; ci[i] = tci; gi[i] = tgi; gl[i] = tgl; gr[i] = tgr;
            dcr[i] = dci[i] = dgi[i] = dgl[i] = dgr[i] = 0.0f;
        }
        else
        {
            dcr[i] = (tcr - cr[i]) * inv;
            dci[i] = (tci - ci[i]) * inv;
            dgi[i] = (tgi - gi[i]) * inv;
            dgl[i] = (tgl - gl[i]) * inv;
            dgr[i] = (tgr - gr[i]) * inv;
        }
    }
    // Modes dropped from the bank stop cleanly next time they are used.
    for (int i = n; i < count; ++i) re[i] = im[i] = 0.0f;
    count = n;
    fresh = false;
}

void ModalBank::process (const float* in, float* outL, float* outR, int n)
{
    const float lim2 = limit * limit;
    for (int s = 0; s < n; ++s)
    {
        const float x = in[s];
        float accL = 0.0f, accR = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            cr[i] += dcr[i]; ci[i] += dci[i];
            gi[i] += dgi[i]; gl[i] += dgl[i]; gr[i] += dgr[i];

            const float nr = re[i] * cr[i] - im[i] * ci[i] + x * gi[i];
            const float ni = re[i] * ci[i] + im[i] * cr[i];
            const float m2 = nr * nr + ni * ni;
            if (m2 > lim2)
            {
                const float k = limit / std::sqrt (m2);
                re[i] = nr * k; im[i] = ni * k;
            }
            else
            {
                re[i] = nr; im[i] = ni;
            }
            accL += im[i] * gl[i];
            accR += im[i] * gr[i];
        }
        outL[s] += accL;
        outR[s] += accR;
    }
}

} // namespace dy::nodal
