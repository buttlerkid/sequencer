#include "NodalEngine.h"

namespace dy::nodal {

void NodalEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    bank.prepare (sr);
    (void) modeSet (Body::Square);        // make sure the mode tables exist before audio starts
    reset();
}

void NodalEngine::reset()
{
    bank.reset();
    hp.reset(); lp.reset();
    subPos = 0;
    first = true;
    for (auto& e : energies) e = 0.0f;
    peakIn = peakOut = 0.0f;
}

void NodalEngine::strike (float velocity)
{
    // ~6 ms of low-passed noise with a fast decay: a felt mallet.
    burstLeft = static_cast<int> (sr * 0.006);
    burstGain = clampT (velocity, 0.0f, 1.0f) * 0.9f;
    burstEnv  = 1.0f;
}

int NodalEngine::dominantMode() const
{
    int best = 0;
    for (int i = 1; i < active; ++i)
        if (energies[i] > energies[best]) best = i;
    return best;
}

void NodalEngine::control()
{
    const auto& p   = params;
    const Body body = static_cast<Body> (clampT (p.body, 0, kNumBodies - 1));
    const auto& ms  = modeSet (body).modes;
    const auto& mat = kMaterials[clampT (p.material, 0, kNumMaterials - 1)];

    // ---- body pitch: snap to the scale, then glide
    const double target = p.snap ? snapToScale (p.pitch, p.key, p.scale) : p.pitch;
    if (first || p.glideMs <= 0.5)
        smoothNote = target;
    else
        smoothNote += (target - smoothNote) * (1.0 - std::exp (-kSubBlock / (p.glideMs * 0.001 * sr)));
    const double f0 = noteToHz (smoothNote);

    // ---- positions
    const Vec3 strike = strikePoint (body, static_cast<float> (p.strikeX), static_cast<float> (p.strikeY));
    Vec3 pl, pr;
    pickupPoints (body, strike, static_cast<float> (p.spread), pl, pr);

    const int n = clampT (p.density, 1, std::min (kMaxModes, static_cast<int> (ms.size())));
    const double alpha = mat.alpha + clampT (p.damping, 0.0, 1.0) * 1.5;
    const double tilt  = clampT (p.brightness, -1.0, 1.0) * 0.5 + mat.bright * 0.5 - 0.35;
    const float  outNorm = 2.0f / std::sqrt (static_cast<float> (n));

    for (int i = 0; i < kMaxModes; ++i)
    {
        auto& t = targets[i];
        if (i >= n || i >= static_cast<int> (ms.size()))
        {
            t.in = 0.0f;                  // no new energy; what rings keeps ringing out
            continue;
        }
        const Mode& m = ms[static_cast<size_t> (i)];
        const double free = f0 * std::pow (m.ratio, 1.0 + mat.stretch);
        double f = lockedFrequency (free, f0, p.tuneMode, p.lock, p.key, p.scale);
        if (p.tuneMode != TuneFree && p.lock > 0.5)
            f *= 1.0 + 0.0007 * ((i % 3) - 1);          // modes landing on one note beat gently, like a real bell

        const double rel = std::max (1.0, f / f0);
        t.freq = f;
        t.t60  = clampT (p.decay * mat.decay * std::pow (rel, -alpha), 0.02, 60.0);
        const float g = static_cast<float> (std::pow (rel, tilt));
        t.in   = modeShape (body, m, strike) * g;
        t.outL = modeShape (body, m, pl) * outNorm;
        t.outR = modeShape (body, m, pr) * outNorm;
        freqs[i] = f;
    }
    active = n;
    bank.setTargets (targets, std::max (active, bank.size()));

    for (int i = 0; i < kMaxModes; ++i)
        energies[i] = i < bank.size() ? bank.amplitude2 (i) : 0.0f;

    // ---- input shaping and gains
    hp.set (clampT (p.lowCut, 10.0, 5000.0), 0.7071, sr);
    lp.set (clampT (p.highCut, 200.0, 22000.0), 0.7071, sr);
    const int rampLen = kSubBlock;
    if (first)
    {
        drive.snap (static_cast<float> (dbToGain (p.driveDb)));
        mix.snap (static_cast<float> (clampT (p.mix, 0.0, 1.0)));
        outGain.snap (static_cast<float> (dbToGain (p.outDb)));
    }
    else
    {
        drive.set (static_cast<float> (dbToGain (p.driveDb)), rampLen);
        mix.set (static_cast<float> (clampT (p.mix, 0.0, 1.0)), rampLen);
        outGain.set (static_cast<float> (dbToGain (p.outDb)), rampLen);
    }
    first = false;
}

void NodalEngine::process (float* left, float* right, int numSamples)
{
    int pos = 0;
    while (pos < numSamples)
    {
        if (subPos == 0) control();
        const int k = std::min (numSamples - pos, kSubBlock - subPos);
        float* L = left + pos;
        float* R = right + pos;

        for (int s = 0; s < k; ++s)
        {
            const float mono = 0.5f * (L[s] + R[s]);
            peakIn = std::max (peakIn, std::abs (mono));
            double high = 0.0;
            (void) hp.process (mono, high);            // high-pass output
            double dummy = 0.0;
            const double band = lp.process (high, dummy);
            exc[s] = static_cast<float> (band) * drive.next();
            if (burstLeft > 0)
            {
                --burstLeft;
                burstLp += 0.35f * (rng.bipolar() - burstLp);
                exc[s] += burstLp * burstGain * burstEnv * 4.0f;
                burstEnv *= 0.9985f;
            }
            wetL[s] = wetR[s] = 0.0f;
        }

        bank.process (exc, wetL, wetR, k);

        for (int s = 0; s < k; ++s)
        {
            const float m = mix.next(), g = outGain.next();
            const float wl = softClip (wetL[s]), wr = softClip (wetR[s]);
            const float ol = (L[s] + (wl - L[s]) * m) * g;
            const float orr = (R[s] + (wr - R[s]) * m) * g;
            L[s] = ol;
            if (R != L) R[s] = orr;
            peakOut = std::max (peakOut, std::max (std::abs (ol), std::abs (orr)));
        }

        pos += k;
        subPos = (subPos + k) % kSubBlock;
    }
}

} // namespace dy::nodal
