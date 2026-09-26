#include "NodalEngine.h"

namespace dy::nodal {

void NodalEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    for (auto& v : voices) v.bank.prepare (sr);
    transient.prepare (sr);
    tracker.prepare (sr);
    (void) modeSet (Body::Square);        // make sure the mode tables exist before audio starts
    reset();
}

void NodalEngine::reset()
{
    uint32_t seed = 0x51ed27u;
    for (auto& v : voices)
    {
        v.bank.reset();
        v.active = v.gate = v.key = false;
        v.note = -1;
        v.env = v.level = v.burstLp = v.noiseLp = v.jitter = v.jitterTarget = v.click = v.burstEnv = 0.0f;
        v.burst = v.holdSamples = v.jitterCount = v.quiet = 0;
        v.fresh = true;
        v.rng = Rng (seed += 0x9e3779b9u);
    }
    hp.reset(); lp.reset();
    for (auto& l : lfos) l.reset();
    envF.reset(); transient.reset(); tracker.reset();
    trackedNote = -1.0;
    onsets = 0;
    subPos = 0;
    first = true;
    lastPlayMode = -1;
    allocCounter = 0;
    heldCount = 0;
    lastNote = -1;
    sustainDown = false;
    bend = 0.0f;
    burstLeft = 0;
    active = 0;
    for (auto& e : energies) e = 0.0f;
    peakIn = peakOut = 0.0f;
}

void NodalEngine::strike (float velocity)
{
    if (params.playMode == PlayInstrument)
    {
        // A tap plays the body's own note with a mallet and lets it ring.
        const int note = clampT (static_cast<int> (std::lround (params.pitch)), 0, 127);
        Voice& v = allocate (note);
        trigger (v, note, velocity, false, ExcMallet);
        v.key = false;
        v.holdSamples = static_cast<int> (sr * 0.35);
        return;
    }
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

int NodalEngine::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

// ------------------------------------------------------------------ MIDI
void NodalEngine::pushHeld (int note)
{
    popHeld (note);
    if (heldCount == 16) { std::copy (held + 1, held + 16, held); --heldCount; }
    held[heldCount++] = note;
}

void NodalEngine::popHeld (int note)
{
    int j = 0;
    for (int i = 0; i < heldCount; ++i) if (held[i] != note) held[j++] = held[i];
    heldCount = j;
}

NodalEngine::Voice& NodalEngine::allocate (int note)
{
    const int poly = clampT (params.polyphony, 1, kMaxVoices);
    for (int i = 0; i < poly; ++i) if (voices[static_cast<size_t> (i)].active && voices[static_cast<size_t> (i)].note == note) return voices[static_cast<size_t> (i)];
    for (int i = 0; i < poly; ++i) if (! voices[static_cast<size_t> (i)].active) return voices[static_cast<size_t> (i)];
    // steal the oldest released voice, else the oldest
    int best = -1;
    uint32_t oldest = 0xffffffffu;
    for (int i = 0; i < poly; ++i)
        if (! voices[static_cast<size_t> (i)].gate && voices[static_cast<size_t> (i)].started < oldest) { oldest = voices[static_cast<size_t> (i)].started; best = i; }
    if (best < 0)
        for (int i = 0; i < poly; ++i)
            if (voices[static_cast<size_t> (i)].started < oldest) { oldest = voices[static_cast<size_t> (i)].started; best = i; }
    return voices[static_cast<size_t> (std::max (0, best))];
}

void NodalEngine::trigger (Voice& v, int note, float velocity, bool legato, int excOverride)
{
    const auto& p = params;
    const int ex = excOverride >= 0 ? excOverride : clampT (p.exciter, 0, kNumExciters - 1);
    const float vs = static_cast<float> (clampT (p.velSens, 0.0, 1.0));
    const float vel = clampT (velocity, 0.0f, 1.0f);
    const float tone = clampT (static_cast<float> (p.excTone) + 0.35f * vs * (vel - 0.6f), 0.0f, 1.0f);
    const bool wasActive = v.active;

    v.active = true;
    v.note = note;
    v.velocity = vel;
    v.gain = 1.0f - vs + vs * vel;
    v.gate = v.key = true;
    v.holdSamples = 0;
    v.quiet = 0;
    v.started = ++allocCounter;
    if (! legato) v.fresh = true;                  // jump to the new pitch (legato glides)
    if (! (legato && v.exciter >= ExcBow && ex >= ExcBow)) v.exciter = ex;

    if (ex == ExcMallet || ex == ExcPluck)
    {
        const bool pluck = ex == ExcPluck;
        const double len = pluck ? 0.001 + 0.002 * (1.0 - tone) : 0.0015 + 0.008 * (1.0 - tone);
        v.burstLen = std::max (1, static_cast<int> (sr * len));
        v.burst = v.burstLen;
        v.burstEnv = 1.0f;
        v.burstC = pluck ? 0.35f + 0.6f * tone : 0.04f + 0.6f * tone * tone;
        // keep the burst's loudness independent of how dark it is (low-passed noise loses power)
        const float c = v.burstC;
        v.burstGain = v.gain * (pluck ? 9.0f : 10.0f) * std::sqrt ((2.0f - c) / c) * 0.577f * (0.55f + 0.45f * tone);
        v.click = pluck ? v.gain * (1.0f + 2.0f * tone) : 0.0f;
    }
    else if (! legato)
        v.env = wasActive ? v.env : 0.0f;

    // A voice that was silent needs its resonators set now, not at the next control
    // tick, or a short burst would be lost.
    if (! wasActive)
    {
        v.bank.reset();
        v.env = 0.0f;
        v.pitch = note + bend + ctlMods[ModPitch] * 12.0;
        if (active > 0) voiceTargets (v, noteToHz (v.pitch), 0.0, true);
        v.fresh = false;
    }
}

void NodalEngine::noteOn (int note, float velocity)
{
    note = clampT (note, 0, 127);
    if (velocity <= 0.0f) { noteOff (note); return; }
    pushHeld (note);
    lastNote = note;
    if (params.playMode != PlayInstrument) return;          // key follow reads lastNote

    if (clampT (params.polyphony, 1, kMaxVoices) == 1)
    {
        Voice& v = voices[0];
        trigger (v, note, velocity, v.active && v.key);
        return;
    }
    trigger (allocate (note), note, velocity, false);
}

void NodalEngine::noteOff (int note)
{
    popHeld (note);
    if (params.playMode != PlayInstrument)
    {
        if (heldCount > 0) lastNote = held[heldCount - 1];   // key follow falls back to the key still held
        return;
    }
    if (clampT (params.polyphony, 1, kMaxVoices) == 1)
    {
        Voice& v = voices[0];
        if (v.active && v.key && v.note == note)
        {
            if (heldCount > 0) v.note = held[heldCount - 1];  // legato back to the previous key
            else { v.key = false; if (! sustainDown) v.gate = false; }
        }
        return;
    }
    for (auto& v : voices)
        if (v.active && v.key && v.note == note)
        {
            v.key = false;
            if (! sustainDown && v.holdSamples <= 0) v.gate = false;
        }
}

void NodalEngine::sustain (bool down)
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.active && ! v.key && v.holdSamples <= 0) v.gate = false;
}

void NodalEngine::pitchBend (float semitones) { bend = clampT (semitones, -24.0f, 24.0f); }

void NodalEngine::allNotesOff()
{
    heldCount = 0;
    sustainDown = false;
    for (auto& v : voices) { v.key = false; v.gate = false; v.holdSamples = 0; }
}

// ------------------------------------------------------------------ control
void NodalEngine::voiceTargets (Voice& v, double f0, double releaseDamp, bool lockFundamental)
{
    const auto& p = params;
    const auto& mat = kMaterials[clampT (p.material, 0, kNumMaterials - 1)];
    const double shortT60 = 0.08;
    for (int i = 0; i < kMaxModes; ++i)
    {
        auto& t = v.targets[i];
        if (i >= active)
        {
            t.in = 0.0f;                  // no new energy; what rings keeps ringing out
            continue;
        }
        const double free = f0 * ratioPow[i];
        double f = (lockFundamental && i == 0) ? free : lockedFrequency (free, f0, p.tuneMode, ctlLock, p.key, p.scale);
        if (p.tuneMode != TuneFree && ctlLock > 0.5)
            f *= 1.0 + 0.0007 * ((i % 3) - 1);          // modes landing on one note beat gently, like a real bell

        const double rel = std::max (1.0, f / f0);
        double t60 = clampT (ctlDecay * mat.decay * std::pow (rel, -ctlAlpha), 0.02, 60.0);
        if (releaseDamp > 0.0 && t60 > shortT60)
            t60 = std::exp (std::log (t60) + releaseDamp * (std::log (shortT60) - std::log (t60)));
        t.freq = f;
        t.t60  = t60;
        t.in   = shapeIn[i] * static_cast<float> (std::pow (rel, ctlTilt));
        t.outL = shapeL[i];
        t.outR = shapeR[i];
        v.freqs[i] = f;
    }
    v.bank.setTargets (v.targets, std::max (active, v.bank.size()));
}

void NodalEngine::control (int blockPos)
{
    const auto& p   = params;
    const Body body = static_cast<Body> (clampT (p.body, 0, kNumBodies - 1));
    const auto& ms  = modeSet (body).modes;
    const auto& mat = kMaterials[clampT (p.material, 0, kNumMaterials - 1)];
    const bool instrument = p.playMode == PlayInstrument;

    // switching between effect and instrument starts from a quiet body
    if (p.playMode != lastPlayMode)
    {
        if (lastPlayMode >= 0 && (lastPlayMode == PlayInstrument) != instrument)
            for (auto& v : voices) { v.bank.reset(); v.active = v.gate = v.key = false; v.note = -1; v.level = 0.0f; v.fresh = true; }
        lastPlayMode = p.playMode;
    }

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
    std::copy (std::begin (mods), std::end (mods), std::begin (ctlMods));

    if (p.trackPitch && tracker.update() && tracker.valid() && tracker.clarity() > 0.8f)
        trackedNote = hzToNote (tracker.frequency());

    // ---- what every voice shares: strike and pickups, mode shapes, damping, tilt
    const float sx = static_cast<float> (clampT (p.strikeX + mods[ModStrikeX], -1.0, 1.0));
    const float sy = static_cast<float> (clampT (p.strikeY + mods[ModStrikeY], -1.0, 1.0));
    const Vec3 strike = strikePoint (body, sx, sy);
    Vec3 pl, pr;
    pickupPoints (body, strike, static_cast<float> (clampT (p.spread + mods[ModSpread], 0.0, 1.0)), pl, pr);

    const int n = clampT (p.density, 1, std::min (kMaxModes, static_cast<int> (ms.size())));
    ctlAlpha = mat.alpha + clampT (p.damping + mods[ModDamping], 0.0, 1.0) * 1.5;
    ctlTilt  = clampT (p.brightness + mods[ModBrightness], -1.0, 1.0) * 0.5 + mat.bright * 0.5 - 0.35;
    ctlDecay = p.decay * std::pow (2.0, mods[ModDecay] * 3.0);
    ctlLock  = clampT (p.lock + mods[ModLock], 0.0, 1.0);
    // instrument voices add up, so each is a little quieter than the effect plate
    ctlOutNorm = 2.0f / std::sqrt (static_cast<float> (n)) * (instrument ? 0.7f : 1.0f);
    for (int i = 0; i < n; ++i)
    {
        const Mode& m = ms[static_cast<size_t> (i)];
        ratioPow[i] = std::pow (m.ratio, 1.0 + mat.stretch);
        shapeIn[i]  = modeShape (body, m, strike);
        shapeL[i]   = modeShape (body, m, pl) * ctlOutNorm;
        shapeR[i]   = modeShape (body, m, pr) * ctlOutNorm;
    }
    active = n;

    // exciter coefficients
    excAtt = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (0.5, p.excAttackMs) * 0.001 * sr)));
    excRel = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (5.0, p.excReleaseMs) * 0.001 * sr)));
    const float tone = static_cast<float> (clampT (p.excTone, 0.0, 1.0));
    noiseC = 0.03f + 0.6f * tone * tone;
    noiseNorm = std::sqrt ((2.0f - noiseC) / noiseC) * 0.577f;

    if (! instrument)
    {
        // ---- one plate: key follow note, else the heard pitch, else the dial; modulate, snap, glide
        double basePitch = p.pitch;
        if (p.trackPitch && trackedNote > 0.0) basePitch = trackedNote;
        if (p.playMode == PlayKeyFollow && lastNote >= 0) basePitch = lastNote + bend;
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
        shownNote = smoothNote;

        Voice& v = voices[0];
        v.active = v.gate = true;
        v.note = -1;
        voiceTargets (v, noteToHz (smoothNote), 0.0, false);
        for (int i = 0; i < kMaxModes; ++i)
        {
            energies[i] = i < v.bank.size() ? v.bank.amplitude2 (i) : 0.0f;
            freqs[i] = v.freqs[i];
        }
    }
    else
    {
        // ---- voices
        const int poly = clampT (p.polyphony, 1, kMaxVoices);
        const double glideK = p.glideMs > 0.5 ? 1.0 - std::exp (-kSubBlock / (p.glideMs * 0.001 * sr)) : 1.0;
        std::fill (std::begin (energies), std::end (energies), 0.0f);
        float loudest = -1.0f;
        int loudV = -1;
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            Voice& v = voices[static_cast<size_t> (vi)];
            if (! v.active) continue;
            if (vi >= poly) v.key = v.gate = false;             // over the voice limit: ring out
            if (v.holdSamples > 0)
            {
                v.holdSamples -= kSubBlock;
                if (v.holdSamples <= 0 && ! v.key && ! sustainDown) v.gate = false;
            }
            const double target = v.note + bend + mods[ModPitch] * 12.0;
            if (v.fresh || poly > 1) v.pitch = target;
            else v.pitch += (target - v.pitch) * glideK;
            v.fresh = false;

            voiceTargets (v, noteToHz (clampT (v.pitch, 0.0, 135.0)), v.gate ? 0.0 : clampT (p.noteDamp, 0.0, 1.0), true);

            float e = 0.0f;
            for (int i = 0; i < v.bank.size(); ++i)
            {
                const float ei = v.bank.amplitude2 (i);
                energies[i] += ei;
                e += ei;
            }
            v.level = clampT (std::sqrt (e) * 2.0f, 0.0f, 1.0f);
            if (e > loudest) { loudest = e; loudV = vi; }

            // retire voices that have rung out
            if (! v.gate && v.burst == 0 && v.env < 1e-4f && e < 1e-9f)
            {
                if (++v.quiet > 4)
                {
                    v.active = false;
                    v.note = -1;
                    v.level = 0.0f;
                    v.bank.reset();
                }
            }
            else
                v.quiet = 0;
        }
        if (loudV >= 0)
        {
            shownNote = voices[static_cast<size_t> (loudV)].pitch;
            for (int i = 0; i < kMaxModes; ++i) freqs[i] = voices[static_cast<size_t> (loudV)].freqs[i];
        }
    }

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

// ------------------------------------------------------------------ exciters
float NodalEngine::excite (Voice& v, float input)
{
    float x = 0.0f;
    if (v.burst > 0)
    {
        v.burstLp += v.burstC * (v.rng.bipolar() - v.burstLp);
        x += v.burstLp * v.burstGain * v.burstEnv + v.click;
        v.click = 0.0f;                                 // a pluck starts with one sharp edge
        v.burstEnv *= 1.0f - 4.0f / static_cast<float> (v.burstLen + 4);
        --v.burst;
    }
    if (v.exciter >= ExcBow)
    {
        v.env += ((v.gate ? 1.0f : 0.0f) > v.env ? excAtt : excRel) * ((v.gate ? 1.0f : 0.0f) - v.env);
        if (v.env > 1e-5f)
        {
            const float g = v.env * v.gain;
            if (v.exciter == ExcInput)
                x += input * g * 2.0f;
            else
            {
                v.noiseLp += noiseC * (v.rng.bipolar() - v.noiseLp);
                const float noise = v.noiseLp * noiseNorm;
                if (v.exciter == ExcBow)
                {
                    // rosin: the bow grabs and slips, so its force comes in uneven waves
                    if (--v.jitterCount <= 0)
                    {
                        v.jitterTarget = v.rng.uniform();
                        v.jitterCount = static_cast<int> (sr / 70.0);
                    }
                    v.jitter += 0.003f * (v.jitterTarget - v.jitter);
                    x += noise * g * (0.40f + 0.64f * v.jitter);
                }
                else
                    x += noise * g * 0.64f;
            }
        }
    }
    return x;
}

// ------------------------------------------------------------------ audio
void NodalEngine::process (float* left, float* right, const float* scLeft, const float* scRight, int numSamples)
{
    const bool haveSc = scLeft != nullptr;
    int pos = 0;
    while (pos < numSamples)
    {
        if (subPos == 0) control (pos);
        const int k = std::min (numSamples - pos, kSubBlock - subPos);
        float* L = left + pos;
        float* R = right + pos;
        const float* SL = haveSc ? scLeft + pos : nullptr;
        const float* SR = haveSc ? (scRight != nullptr ? scRight + pos : SL) : nullptr;
        const int scMode = haveSc ? params.sidechain : ScOff;
        const bool instrument = params.playMode == PlayInstrument;

        for (int s = 0; s < k; ++s)
        {
            const float mono = 0.5f * (L[s] + R[s]);
            const float side = haveSc ? 0.5f * (SL[s] + SR[s]) : 0.0f;
            const float heard  = scMode == ScEnvelope ? side : mono;       // what the follower / detectors listen to
            const float source = scMode == ScExcites ? side : mono;        // what excites the body
            peakIn = std::max (peakIn, std::max (std::abs (mono), scMode != ScOff ? std::abs (side) : 0.0f));
            envF.push (heard);
            if (transient.push (heard)) ++onsets;
            if (params.trackPitch) tracker.push (heard);
            double high = 0.0;
            (void) hp.process (source, high);            // high-pass output
            double dummy = 0.0;
            const double band = lp.process (high, dummy);
            exc[s] = static_cast<float> (band) * drive.next();
            wetL[s] = wetR[s] = 0.0f;
        }

        if (! instrument)
        {
            for (int s = 0; s < k; ++s)
            {
                vexc[s] = exc[s];
                if (burstLeft > 0)
                {
                    --burstLeft;
                    burstLp += 0.35f * (rng.bipolar() - burstLp);
                    vexc[s] += burstLp * burstGain * burstEnv * 4.0f;
                    burstEnv *= 0.9985f;
                }
            }
            voices[0].bank.process (vexc, wetL, wetR, k);
        }
        else
        {
            for (auto& v : voices)
            {
                if (! v.active) continue;
                for (int s = 0; s < k; ++s) vexc[s] = excite (v, exc[s]);
                v.bank.process (vexc, wetL, wetR, k);
            }
        }

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
