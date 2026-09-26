#include "NodalEngine.h"

namespace dy::nodal {

void NodalEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    bank.prepare (sr);
    transient.prepare (sr);
    tracker.prepare (sr);
    (void) modeSet (Body::Square);        // make sure the mode tables exist before audio starts
    reset();
}

void NodalEngine::reset()
{
    bank.reset();
    hp.reset(); lp.reset();
    for (auto& l : lfos) l.reset();
    envF.reset(); transient.reset(); tracker.reset();
    trackedNote = -1.0;
    onsets = 0;
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

void NodalEngine::control (int blockPos)
{
    const auto& p   = params;
    const Body body = static_cast<Body> (clampT (p.body, 0, kNumBodies - 1));
    const auto& ms  = modeSet (body).modes;
    const auto& mat = kMaterials[clampT (p.material, 0, kNumMaterials - 1)];

    // ---- modulation sources
    float mods[kNumModTargets] {};
    const double ppqNow = transport.ppq + blockPos * transport.bpm / 60.0 / sr;
    for (int i = 0; i < 2; ++i)
    {
        lfoVals[i] = lfos[i].advance (p.lfo[i], transport, ppqNow, kSubBlock, sr);
        if (p.lfo[i].target > ModOff && p.lfo[i].target < kNumModTargets)
            mods[p.lfo[i].target] += lfoVals[i] * static_cast<float> (clampT (p.lfo[i].amount, -1.0, 1.0));
    }
    envF.setTimes (p.env.attackMs, p.env.releaseMs, sr);
    envVal = envF.normalised();
    if (p.env.target > ModOff && p.env.target < kNumModTargets)
        mods[p.env.target] += envVal * static_cast<float> (clampT (p.env.amount, -1.0, 1.0));

    std::copy (std::begin (mods), std::end (mods), std::begin (modVals));

    if (p.trackPitch && tracker.update() && tracker.valid() && tracker.clarity() > 0.8f)
        trackedNote = hzToNote (tracker.frequency());

    // ---- body pitch: follow the input if asked, modulate, snap to the scale, then glide
    const double basePitch = (p.trackPitch && trackedNote > 0.0) ? trackedNote : p.pitch;
    double target;
    if (p.snap)
        target = stepScale (basePitch, std::round (mods[ModPitch] * 14.0), p.key, p.scale);   // +-2 octaves of scale steps
    else
        target = basePitch + mods[ModPitch] * 24.0;
    target = clampT (target, 12.0, 120.0);
    if (first || p.glideMs <= 0.5)
        smoothNote = target;
    else
        smoothNote += (target - smoothNote) * (1.0 - std::exp (-kSubBlock / (p.glideMs * 0.001 * sr)));
    const double f0 = noteToHz (smoothNote);

    // ---- positions
    const float sx = static_cast<float> (clampT (p.strikeX + mods[ModStrikeX], -1.0, 1.0));
    const float sy = static_cast<float> (clampT (p.strikeY + mods[ModStrikeY], -1.0, 1.0));
    const Vec3 strike = strikePoint (body, sx, sy);
    Vec3 pl, pr;
    pickupPoints (body, strike, static_cast<float> (clampT (p.spread + mods[ModSpread], 0.0, 1.0)), pl, pr);

    const int n = clampT (p.density, 1, std::min (kMaxModes, static_cast<int> (ms.size())));
    const double alpha = mat.alpha + clampT (p.damping + mods[ModDamping], 0.0, 1.0) * 1.5;
    const double tilt  = clampT (p.brightness + mods[ModBrightness], -1.0, 1.0) * 0.5 + mat.bright * 0.5 - 0.35;
    const double decay = p.decay * std::pow (2.0, mods[ModDecay] * 3.0);
    const double lock  = clampT (p.lock + mods[ModLock], 0.0, 1.0);
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
        double f = lockedFrequency (free, f0, p.tuneMode, lock, p.key, p.scale);
        if (p.tuneMode != TuneFree && lock > 0.5)
            f *= 1.0 + 0.0007 * ((i % 3) - 1);          // modes landing on one note beat gently, like a real bell

        const double rel = std::max (1.0, f / f0);
        t.freq = f;
        t.t60  = clampT (decay * mat.decay * std::pow (rel, -alpha), 0.02, 60.0);
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
    const float driveG = static_cast<float> (dbToGain (clampT (p.driveDb + mods[ModDrive] * 24.0, -48.0, 36.0)));
    const float mixV   = static_cast<float> (clampT (p.mix + mods[ModMix], 0.0, 1.0));
    if (first)
    {
        drive.snap (driveG);
        mix.snap (mixV);
        outGain.snap (static_cast<float> (dbToGain (p.outDb)));
    }
    else
    {
        drive.set (driveG, rampLen);
        mix.set (mixV, rampLen);
        outGain.set (static_cast<float> (dbToGain (p.outDb)), rampLen);
    }
    first = false;
}

void NodalEngine::process (float* left, float* right, int numSamples)
{
    int pos = 0;
    while (pos < numSamples)
    {
        if (subPos == 0) control (pos);
        const int k = std::min (numSamples - pos, kSubBlock - subPos);
        float* L = left + pos;
        float* R = right + pos;

        for (int s = 0; s < k; ++s)
        {
            const float mono = 0.5f * (L[s] + R[s]);
            peakIn = std::max (peakIn, std::abs (mono));
            envF.push (mono);
            if (transient.push (mono)) ++onsets;
            if (params.trackPitch) tracker.push (mono);
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
