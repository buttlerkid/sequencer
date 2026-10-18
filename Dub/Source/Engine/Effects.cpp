#include "Effects.h"

namespace dy::dub {

// ------------------------------------------------------------------ delay
void DubDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    const int n = static_cast<int> (sr * 4.2) + 8;              // up to a half note at 30 bpm
    l.resize (n);
    r.resize (n);
    glide = 1.0 - std::exp (-1.0 / (0.12 * sr));                 // ~120 ms tape glide
    reset();
}

void DubDelay::reset()
{
    l.clear(); r.clear();
    hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
    time = timeTarget;
}

void DubDelay::set (double timeSamples, float feedback, float hpHz, float lpHz, float pingPong)
{
    timeTarget = clampT (timeSamples, 2.0, sr * 4.0);
    fb = clampT (feedback, 0.0f, 1.0f);
    hpL.setLowpass (hpHz, sr); hpR.a = hpL.a;
    lpL.setLowpass (lpHz, sr); lpR.a = lpL.a;
    pp = clampT (pingPong, 0.0f, 1.0f);
}

void DubDelay::process (float inL, float inR, float& outL, float& outR)
{
    time += glide * (timeTarget - time);
    const float yl = l.read (time), yr = r.read (time);
    // a little over 1 still saturates instead of exploding: the dub runaway stays musical
    const float g = fb * 1.02f;
    const float fl = softClip (lpL.lp (hpL.hp (yl)) * g);
    const float fr = softClip (lpR.lp (hpR.hp (yr)) * g);
    // blend of a stereo echo and a ping-pong, where everything enters on the left and
    // bounces L -> R -> L; the feedback matrix stays unitary-ish (eigenvalues 1 and 1 - 2p)
    const float mono = (inL + inR) * 0.5f;
    l.push ((1.0f - pp) * (inL + fl) + pp * (mono + fr));
    r.push ((1.0f - pp) * (inR + fr) + pp * fl);
    outL = yl;
    outR = yr;
}

// ------------------------------------------------------------------ reverb
namespace {
    // base line lengths in ms (mutually prime-ish), and per type: scale, decay seconds, diffusion, damping, pre-delay ms
    constexpr double kLineMs[8] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.1, 73.3 };
    constexpr double kDiffMs[4] = { 4.7, 3.6, 12.7, 9.3 };
    struct Shape { double scale, t60, diffusion, damping, preMs; };
    constexpr Shape kShapes[kNumReverbTypes] = {
        { 0.32, 0.55, 0.50, 0.55, 2.0 },     // room
        { 0.55, 2.20, 0.75, 0.25, 0.5 },     // plate: dense and bright
        { 1.35, 3.40, 0.70, 0.50, 22.0 },    // hall
        { 0.50, 2.00, 0.30, 0.40, 0.0 },     // spring (FDN unused)
        { 0.80, 1.40, 0.62, 0.45, 9.0 },     // chamber
    };
}

void DubReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& d : lines) d.resize (static_cast<int> (sr * 0.2) + 8);
    for (int i = 0; i < 4; ++i)
    {
        diff[static_cast<size_t> (i)].resize (static_cast<int> (sr * 0.02) + 8);
        diffLen[static_cast<size_t> (i)] = std::max (1, static_cast<int> (kDiffMs[i] * 0.001 * sr));
    }
    pre.resize (static_cast<int> (sr * 0.3) + 8);
    for (auto& s : springs) s.loop.resize (static_cast<int> (sr * 0.12) + 8);
    lastSize = lastDecay = lastTone = -1.0f;
    set (type, 0.5f, 0.5f, 0.5f);
    for (int i = 0; i < N; ++i) len[static_cast<size_t> (i)] = lenTarget[static_cast<size_t> (i)];
    preNow = preLen;
    reset();
}

void DubReverb::reset()
{
    for (auto& d : lines) d.clear();
    for (auto& d : diff) d.clear();
    for (auto& d : damp) d.reset();
    pre.clear();
    for (auto& s : springs) { s.loop.clear(); s.apZ.fill (0.0f); s.lp.reset(); s.hp.reset(); }
}

void DubReverb::set (int t, float size, float decay, float tone, float w, float preMs)
{
    t = clampT (t, 0, kNumReverbTypes - 1);
    width = clampT (w, 0.0f, 1.0f);
    if (t != type) { type = t; lastSize = -1.0f; }
    if (std::abs (preMs - extraPreMs) > 0.05f) { extraPreMs = preMs; lastSize = -1.0f; }
    if (std::abs (size - lastSize) < 0.002f && std::abs (decay - lastDecay) < 0.002f && std::abs (tone - lastTone) < 0.002f) return;
    lastSize = size; lastDecay = decay; lastTone = tone;

    const auto& sh = kShapes[type];
    const double scale = sh.scale * (0.55 + 0.9 * size);
    const double t60 = sh.t60 * std::pow (4.0, decay * 2.0 - 1.0) * (0.7 + 0.6 * size);    // decay: x0.25 .. x4
    const double dampHz = 1500.0 * std::pow (2.0, (1.0 - sh.damping) * 3.0 + (tone - 0.5) * 3.0);
    for (int i = 0; i < N; ++i)
    {
        const double samples = clampT (kLineMs[i] * scale * 0.001 * sr, 8.0, sr * 0.19);
        lenTarget[static_cast<size_t> (i)] = samples;
        gain[static_cast<size_t> (i)] = static_cast<float> (std::pow (10.0, -3.0 * samples / (t60 * sr)));
        damp[static_cast<size_t> (i)].setLowpass (dampHz, sr);
    }
    diffG = static_cast<float> (sh.diffusion);
    preLen = std::max (1, static_cast<int> ((sh.preMs + clampT (extraPreMs, 0.0f, 250.0f)) * 0.001 * sr));

    // spring: two coils of slightly different length; decay sets the loop gain
    const double springT60 = 1.2 * std::pow (3.0, decay * 2.0 - 1.0);
    for (int k = 0; k < 2; ++k)
    {
        auto& s = springs[static_cast<size_t> (k)];
        s.len = (0.034 + 0.03 * size + k * 0.0067) * sr;
        s.g = static_cast<float> (std::pow (10.0, -3.0 * s.len / (springT60 * sr)));
        s.lp.setLowpass (2500.0 + 3500.0 * tone, sr);
        s.hp.setLowpass (180.0, sr);
    }
    springA = 0.55f + 0.15f * size;
}

void DubReverb::process (float inL, float inR, float& outL, float& outR)
{
    // pre-delay (glides when changed, like the size)
    pre.push ((inL + inR) * 0.5f);
    preNow += 0.0005 * (preLen - preNow);
    float x = pre.read (preNow);

    if (type == RevSpring)
    {
        float o[2];
        for (int k = 0; k < 2; ++k)
        {
            auto& s = springs[static_cast<size_t> (k)];
            float y = s.loop.read (s.len);
            // a chain of first-order allpasses smears high frequencies in time: the chirp
            for (auto& z : s.apZ)
            {
                const float v = y - springA * z;
                const float a = z + springA * v;
                z = v;
                y = a;
            }
            const float fbk = s.lp.lp (s.hp.hp (y));
            s.loop.push (softClip (x * 0.6f + fbk * s.g));
            o[k] = y;
        }
        widen (o[0] * 0.9f, o[1] * 0.9f, outL, outR);
        return;
    }

    // input diffusion
    for (int i = 0; i < 4; ++i)
    {
        auto& d = diff[static_cast<size_t> (i)];
        const float delayed = d.read (diffLen[static_cast<size_t> (i)]);
        const float v = x + diffG * delayed;
        d.push (v);
        x = delayed - diffG * v;
    }

    std::array<float, N> y {};
    float sum = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        auto& L = len[static_cast<size_t> (i)];
        L += 0.0008 * (lenTarget[static_cast<size_t> (i)] - L);          // size changes glide, no clicks
        y[static_cast<size_t> (i)] = damp[static_cast<size_t> (i)].lp (lines[static_cast<size_t> (i)].read (L)) * gain[static_cast<size_t> (i)];
        sum += y[static_cast<size_t> (i)];
    }
    // Householder feedback: every line feeds every other line
    const float h = sum * (2.0f / N);
    for (int i = 0; i < N; ++i)
    {
        const float in = (i & 1) ? x : -x;
        lines[static_cast<size_t> (i)].push (y[static_cast<size_t> (i)] - h + in * 0.35f);
    }
    widen ((y[0] - y[2] + y[4] - y[6]) * 0.6f, (y[1] - y[3] + y[5] - y[7]) * 0.6f, outL, outR);
}

void DubReverb::widen (float l, float r, float& outL, float& outR) const
{
    const float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * width;
    outL = mid + side;
    outR = mid - side;
}

} // namespace dy::dub
