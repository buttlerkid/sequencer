#include "DubEngine.h"
#include <cstring>

namespace dy::dub {

void DubEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    delay.prepare (sr);
    reverb.prepare (sr);
    reset();
}

void DubEngine::reset()
{
    for (auto& f : filt) f.reset();
    delay.reset();
    reverb.reset();
    gainL.snap (1.0f); gainR.snap (1.0f); dly.snap (0.0f); rev.snap (0.0f); dryMix.snap (0.0f); wetMix.snap (1.0f);
    lfoPhase.fill (0.0);
    follow = 0.0f;
}

StepPos DubEngine::locate (double ppq) const
{
    StepPos p;
    const double stepBeats = kRates[clampT (params.rate, 0, kNumRates - 1)].beats;
    const double u = ppq / stepBeats;
    // swing: every second step starts late; 0.5 puts it three quarters through the pair
    const double split = 1.0 + clampT (static_cast<double> (params.swing), 0.0, 0.5);
    const double pair = std::floor (u * 0.5);
    const double w = u - 2.0 * pair;
    if (w < split) { p.absStep = static_cast<int64_t> (pair) * 2;     p.frac = static_cast<float> (w / split); }
    else           { p.absStep = static_cast<int64_t> (pair) * 2 + 1; p.frac = static_cast<float> ((w - split) / (2.0 - split)); }

    const int len = clampT (params.length, 1, kSteps);
    int64_t pass = p.absStep / len;
    if (p.absStep < 0 && p.absStep % len != 0) --pass;                      // floor division before the song start
    p.step = static_cast<int> (p.absStep - pass * len);
    const char* letters = kPatterns[clampT (params.pattern, 0, kNumPatterns - 1)];
    const int64_t nl = static_cast<int64_t> (std::strlen (letters));
    const int64_t li = ((pass % nl) + nl) % nl;
    p.bank = letters[li] == 'B' ? 1 : 0;

    const auto& bank = banks[static_cast<size_t> (p.bank)];
    int b0 = p.step, b1 = p.step;
    while (b0 > 0 && bank[static_cast<size_t> (b0 - 1)].chain) --b0;
    while (b1 < len - 1 && bank[static_cast<size_t> (b1)].chain) ++b1;
    p.blockStart = b0;
    p.blockLen = b1 - b0 + 1;
    p.blockPhase = clampT ((static_cast<float> (p.step - b0) + p.frac) / static_cast<float> (p.blockLen), 0.0f, 1.0f);
    p.blockAbs = p.absStep - (p.step - b0);
    return p;
}

bool DubEngine::blockPlays (const Step& s, int64_t abs, int bank) const
{
    if (! s.on) return false;
    if (s.prob >= 0.999f) return true;
    return hash01 (abs, static_cast<uint64_t> (bank) + 17u) < s.prob;
}

float DubEngine::lfoValue (int i, double /*ppq*/) const { return lfoNow[static_cast<size_t> (clampT (i, 0, kNumLfos - 1))]; }

static float lfoShape (int shape, double phase, int64_t cycle, int lfoIndex)
{
    const double ph = phase - std::floor (phase);
    switch (shape)
    {
        case ShapeSine:       return static_cast<float> (std::sin (2.0 * kPi * ph));
        case ShapeTriangle:   return static_cast<float> (1.0 - 4.0 * std::abs (ph - 0.5));
        case ShapeSawUp:      return static_cast<float> (2.0 * ph - 1.0);
        case ShapeSawDown:    return static_cast<float> (1.0 - 2.0 * ph);
        case ShapeSquare:     return ph < 0.5 ? 1.0f : -1.0f;
        case ShapeSampleHold: return hash01 (cycle, 101u + static_cast<uint64_t> (lfoIndex)) * 2.0f - 1.0f;
        default:              return 0.0f;
    }
}

void DubEngine::control (double ppqEnd, double bpm, int n, bool running)
{
    // modulation sources
    for (int i = 0; i < kNumLfos; ++i)
    {
        const auto& s = params.lfo[static_cast<size_t> (i)];
        auto& ph = lfoPhase[static_cast<size_t> (i)];
        if (s.sync) ph = ppqEnd / kLfoDivs[clampT (s.div, 0, kNumLfoDivs - 1)].beats;     // locked to the song (or our own clock)
        else        ph += s.rateHz * n / sr;
        lfoNow[static_cast<size_t> (i)] = lfoShape (s.shape, ph, static_cast<int64_t> (std::floor (ph)), i);
        telemetry.source[static_cast<size_t> (i)].store (lfoNow[static_cast<size_t> (i)]);
    }
    const auto& fs = params.follower;
    followAtt = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (0.1, fs.attackMs) * 0.001 * sr)));
    followRel = static_cast<float> (1.0 - std::exp (-1.0 / (std::max (1.0, fs.releaseMs) * 0.001 * sr)));
    const float db = follow > 1e-6f ? 20.0f * std::log10 (follow) : -120.0f;
    const float folNorm = clampT ((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    telemetry.source[kNumLfos].store (folNorm);

    std::array<float, kNumTargets> norm {};
    for (int t = 1; t < kNumTargets; ++t)
    {
        const auto& d = targetDef (t);
        norm[static_cast<size_t> (t)] = (params.target[static_cast<size_t> (t)] - d.lo) / (d.hi - d.lo);
    }
    for (int i = 0; i < kNumLfos; ++i)
    {
        const auto& s = params.lfo[static_cast<size_t> (i)];
        if (s.target > ModOff && s.target < kNumTargets) norm[static_cast<size_t> (s.target)] += s.amount * lfoNow[static_cast<size_t> (i)];
    }
    if (fs.target > ModOff && fs.target < kNumTargets) norm[static_cast<size_t> (fs.target)] += fs.amount * folNorm;
    for (int t = 1; t < kNumTargets; ++t)
    {
        const auto& d = targetDef (t);
        const float v = clampT (norm[static_cast<size_t> (t)], 0.0f, 1.0f);
        telemetry.modNorm[static_cast<size_t> (t)].store (v);
        mod[static_cast<size_t> (t)] = d.lo + v * (d.hi - d.lo);
    }

    // the step
    float g = 1.0f, pan = mod[ModPan], cutOct = 10.0f + mod[ModCutoff], res = mod[ModRes];
    float dSend = 0.0f, rSend = 0.0f;
    if (running)
    {
        const auto p = locate (ppqEnd);
        const auto& st = banks[static_cast<size_t> (p.bank)][static_cast<size_t> (p.blockStart)];
        const bool plays = blockPlays (st, p.blockAbs, p.bank);
        g = plays ? st.vol * st.venv.at (p.blockPhase) * mod[ModLevel] : 0.0f;
        pan += st.pan;
        cutOct = st.cut * 10.0f + mod[ModCutoff] + st.fdepth * mod[ModEnvDepth] * st.fenv.at (p.blockPhase);
        res += st.res;
        dSend = plays && st.dly ? 1.0f : 0.0f;
        rSend = plays && st.rev ? 1.0f : 0.0f;
        telemetry.bank.store (p.bank);
        telemetry.step.store (p.step);
        telemetry.blockStart.store (p.blockStart);
        telemetry.blockLen.store (p.blockLen);
        telemetry.blockPhase.store (p.blockPhase);
        telemetry.played.store (plays);
    }
    else
    {
        // stopped: the input passes as it is; only the tails ring out
        pan = 0.0f;
        cutOct = 11.0f;
        res = 0.0f;
        telemetry.step.store (-1);
    }
    telemetry.running.store (running);

    pan = clampT (pan, -1.0f, 1.0f);
    const float a = static_cast<float> ((pan + 1.0f) * kPi * 0.25);
    gainL.to (g * std::cos (a) * 1.41421356f, n);
    gainR.to (g * std::sin (a) * 1.41421356f, n);
    dly.to (dSend, n);
    rev.to (rSend, n);
    dryMix.to (1.0f - mod[ModMix], n);
    wetMix.to (mod[ModMix], n);

    const bool bypass = ! running || (params.filterMode == FilterLP && cutOct >= 10.0f && res <= 0.001f);
    filterBypass = bypass;
    cutHz = static_cast<float> (cutToHz (clampT (cutOct, 0.0f, 10.0f)));
    q = static_cast<float> (0.5 * std::pow (24.0, clampT (res, 0.0f, 1.0f)));
    for (auto& f : filt) f.set (cutHz, q, sr);

    const double delayBeats = kDelayTimes[clampT (params.delayTime, 0, kNumDelayTimes - 1)].beats;
    delay.set (delayBeats * 60.0 / std::max (20.0, bpm) * sr, mod[ModDelayFb], params.delayHpHz, params.delayLpHz, params.pingPong);
    reverb.set (params.revType, mod[ModRevSize], params.revDecay, params.revTone);
}

void DubEngine::process (float* L, float* R, int n, double ppq, double bpm, bool running)
{
    const double beatsPerSample = bpm / 60.0 / sr;
    const float out = static_cast<float> (dbToGain (params.outputDb));
    const int mode = clampT (params.filterMode, 0, 2);
    for (int start = 0; start < n; start += kSubBlock)
    {
        const int len = std::min (kSubBlock, n - start);
        control (ppq + (start + len) * beatsPerSample, bpm, len, running);
        for (int i = start; i < start + len; ++i)
        {
            const float xl = L[i], xr = R != nullptr ? R[i] : xl;
            const float pk = std::max (std::abs (xl), std::abs (xr));
            follow += (pk > follow ? followAtt : followRel) * (pk - follow);
            inPk = std::max (inPk, pk);

            float yl = xl, yr = xr;
            if (! filterBypass)
            {
                yl = static_cast<float> (filt[0].process (xl, mode));
                yr = static_cast<float> (filt[1].process (xr, mode));
            }
            yl *= gainL.next();
            yr *= gainR.next();

            const float ds = dly.next(), rs = rev.next();
            float dl, dr, rl, rr;
            delay.process (yl * ds, yr * ds, dl, dr);
            reverb.process (yl * rs, yr * rs, rl, rr);

            const float dm = dryMix.next(), wm = wetMix.next();
            const float wl = yl + dl * mod[ModDelayLevel] + rl * mod[ModRevLevel];
            const float wr = yr + dr * mod[ModDelayLevel] + rr * mod[ModRevLevel];
            float ol = (xl * dm + wl * wm) * out, orr = (xr * dm + wr * wm) * out;
            if (! std::isfinite (ol) || ! std::isfinite (orr))
            {
                for (auto& f : filt) f.reset();
                delay.reset(); reverb.reset();
                ol = orr = 0.0f;
            }
            outPk = std::max (outPk, std::max (std::abs (ol), std::abs (orr)));
            L[i] = ol;
            if (R != nullptr) R[i] = orr;
        }
    }
    peakCount += n;
    if (peakCount > static_cast<int> (sr / 30.0))
    {
        telemetry.inPeak.store (inPk);
        telemetry.outPeak.store (outPk);
        inPk = outPk = 0.0f;
        peakCount = 0;
    }
}

} // namespace dy::dub
