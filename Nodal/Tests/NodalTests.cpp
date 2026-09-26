// DY Nodal engine tests. Same style as the sequencer's: a tiny CHECK macro, no
// framework, no JUCE, so they build and run in seconds.
#include "Engine/Bodies.h"
#include "Engine/ModalBank.h"
#include "Engine/NodalEngine.h"
#include "Engine/Tuning.h"

#include <cstdio>
#include <cmath>
#include <vector>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

using namespace dy::nodal;

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; to = std::min (to, v.size());
    for (size_t i = from; i < to; ++i) s += double (v[i]) * v[i];
    return std::sqrt (s / std::max<size_t> (1, to - from));
}

// Frequency from rising zero crossings with linear interpolation.
static double zeroCrossingHz (const std::vector<float>& v, size_t from, size_t to, double sr)
{
    double first = -1, last = -1; int n = 0;
    for (size_t i = from + 1; i < to && i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (i - 1) + v[i - 1] / (v[i - 1] - v[i]);
            if (first < 0) first = t; else ++n;
            last = t;
        }
    return n > 0 ? sr * n / (last - first) : 0.0;
}

// ------------------------------------------------------------------ special functions + bodies
static void testBessel()
{
    std::puts ("Bessel");
    CHECK_NEAR (besselJ (0, 1.0), 0.7651976865579666, 1e-10);
    CHECK_NEAR (besselJ (1, 2.0), 0.5767248077568734, 1e-10);
    CHECK_NEAR (besselJ (0, 2.404825557695773), 0.0, 1e-9);
    CHECK_NEAR (besselJ (1, 3.831705970207512), 0.0, 1e-9);
    CHECK_NEAR (besselJ (5, 8.771483815959954), 0.0, 1e-9);
}

static void testBodies()
{
    std::puts ("Bodies + mode tables");
    for (int b = 0; b < kNumBodies; ++b)
    {
        const Body body = static_cast<Body> (b);
        const auto& ms = modeSet (body).modes;
        CHECK (static_cast<int> (ms.size()) == kModeTable);
        CHECK_NEAR (ms.front().ratio, 1.0, 1e-12);
        for (size_t i = 1; i < ms.size(); ++i) CHECK (ms[i].ratio >= ms[i - 1].ratio);
        CHECK (ms.back().ratio > 3.0);

        // Normalised shapes peak at ~1 over the body and never exceed it by much.
        for (size_t i = 0; i < 12; ++i)
        {
            double mx = 0;
            if (is3D (body))
            {
                for (int k = 0; k < 4000; ++k)
                {
                    const double y = 1.0 - 2.0 * (k + 0.37) / 4000, r = std::sqrt (1 - y * y), t = 2.39996 * k;
                    mx = std::max (mx, double (std::abs (modeShape (body, ms[i], Vec3 (float (r * std::cos (t)), float (y), float (r * std::sin (t)))))));
                }
            }
            else
            {
                for (int iy = 0; iy < 150; ++iy)
                    for (int ix = 0; ix < 150; ++ix)
                        mx = std::max (mx, double (std::abs (modeShape (body, ms[i], Vec3 (-1 + 2 * ix / 149.0f, -1 + 2 * iy / 149.0f, 0)))));
            }
            if (! (mx > 0.9 && mx < 1.05)) std::printf ("    body %d mode %d (a %d b %d) max %.3f\n", b, int (i), ms[i].a, ms[i].b, mx);
            CHECK (mx > 0.9 && mx < 1.05);
        }
    }

    // 2D shapes vanish outside the plate
    const auto& sq = modeSet (Body::Circle).modes;
    CHECK (modeShape (Body::Circle, sq[3], Vec3 (0.9f, 0.9f, 0)) == 0.0f);
    CHECK (insidePlate (Body::Violin, 0.0f, 0.0f));
    CHECK (! insidePlate (Body::Violin, 0.64f, 0.03f));          // inside a C-bout cut
    CHECK (insidePlate (Body::Hexagon, 0.99f, 0.0f) && ! insidePlate (Body::Hexagon, 0.0f, 0.9f));

    // circle (0, 1), free edge: one nodal circle inside the plate at 2.405 / 3.832 of the radius,
    // and the rim swings (not a nodal line)
    int seen = 0;
    for (const auto& m : modeSet (Body::Circle).modes)
        if (m.a == 0 && m.b == 1)
        {
            ++seen;
            CHECK_NEAR (m.besselZero, 3.8317059702, 1e-6);
            CHECK_NEAR (modeShape (Body::Circle, m, Vec3 (0.62763f, 0, 0)), 0.0f, 0.01f);
            CHECK (std::abs (modeShape (Body::Circle, m, Vec3 (0.999f, 0, 0))) > 0.2f);
        }
    CHECK (seen == 1);
    for (const auto& m : modeSet (Body::Circle).modes)
        if (m.a == 1 && m.b == 1) CHECK_NEAR (m.besselZero, 1.8411837813, 1e-6);    // lowest free-edge circle mode

    // sphere l = 1, m = 0 is the z axis dipole
    for (const auto& m : modeSet (Body::Sphere).modes)
        if (m.a == 1 && m.b == 0) { CHECK_NEAR (std::abs (modeShape (Body::Sphere, m, Vec3 (0, 0, 1))), 1.0f, 1e-3f); CHECK_NEAR (modeShape (Body::Sphere, m, Vec3 (1, 0, 0)), 0.0f, 1e-5f); }

    // shells
    const Vec3 c = surfacePoint (Body::Cube, Vec3 (0.3f, -0.8f, 0.2f));
    CHECK_NEAR (std::max ({ std::abs (c.x), std::abs (c.y), std::abs (c.z) }), 0.57735f, 1e-4f);
    for (int k = 0; k < 200; ++k)
    {
        const Vec3 d = Vec3 (std::sin (k * 1.3f), std::cos (k * 0.7f), std::sin (k * 2.1f + 0.4f));
        const float r = surfacePoint (Body::Icosahedron, d).length();
        CHECK (r >= 0.79f && r <= 1.0001f);
    }

    // strike / pickups stay inside
    for (int b = 0; b < 4; ++b)
        for (float sx = -1; sx <= 1.01f; sx += 0.25f)
            for (float sy = -1; sy <= 1.01f; sy += 0.25f)
            {
                const Body body = static_cast<Body> (b);
                const Vec3 s = strikePoint (body, sx, sy);
                Vec3 l, r;
                pickupPoints (body, s, 0.7f, l, r);
                CHECK (insidePlate (body, s.x, s.y) && insidePlate (body, l.x, l.y) && insidePlate (body, r.x, r.y));
            }
}

// ------------------------------------------------------------------ tuning
static void testTuning()
{
    std::puts ("Tuning");
    CHECK_NEAR (snapToScale (61.0, 0, 1), 60.0, 1e-9);          // C# is between C and D in C major: ties go down
    CHECK_NEAR (snapToScale (61.2, 0, 1), 62.0, 1e-9);
    CHECK_NEAR (snapToScale (64.4, 0, 1), 64.0, 1e-9);
    CHECK_NEAR (snapToScale (47.0, 0, 1), 47.0, 1e-9);          // B2
    CHECK_NEAR (snapToScale (71.9, 0, 1), 72.0, 1e-9);          // rounds up into the next octave
    CHECK_NEAR (snapToScale (61.0, 2, 2), 60.0, 1e-9);          // D minor: C
    CHECK_NEAR (snapToScale (64.0, 0, 12), 63.8631, 1e-4);      // just major third
    CHECK_NEAR (snapToScale (-3.0, 0, 1), -3.0, 1e-9);          // A in octave -2
    CHECK_NEAR (stepScale (60.0, 2, 0, 1), 64.0, 1e-9);
    CHECK_NEAR (stepScale (60.0, -1, 0, 1), 59.0, 1e-9);
    CHECK_NEAR (stepScale (60.0, 7, 0, 1), 72.0, 1e-9);
    CHECK_NEAR (stepScale (60.0, 0.5, 0, 1), 61.0, 1e-9);       // halfway C -> D
    CHECK_NEAR (lockedFrequency (330.0, 100.0, TuneHarmonic, 1.0, 0, 1), 300.0, 1e-9);
    CHECK_NEAR (lockedFrequency (330.0, 100.0, TuneHarmonic, 0.5, 0, 1), std::sqrt (330.0 * 300.0), 1e-6);
    CHECK_NEAR (lockedFrequency (450.0, 100.0, TuneScale, 1.0, 9, 1), 440.0, 1e-6);   // A major, 450 Hz -> A4
    CHECK_NEAR (lockedFrequency (450.0, 100.0, TuneFree, 1.0, 9, 1), 450.0, 1e-12);
}

// ------------------------------------------------------------------ modal bank
static std::vector<float> runBank (ModalBank& bank, ModeTarget t, const std::vector<float>& in, int count = 1)
{
    std::vector<float> outL (in.size(), 0.0f), outR (in.size(), 0.0f);
    std::vector<ModeTarget> ts (static_cast<size_t> (count), t);
    for (size_t pos = 0; pos < in.size(); pos += kSubBlock)
    {
        bank.setTargets (ts.data(), count);
        const int n = static_cast<int> (std::min<size_t> (kSubBlock, in.size() - pos));
        bank.process (in.data() + pos, outL.data() + pos, outR.data() + pos, n);
    }
    return outL;
}

static void testModalBank()
{
    std::puts ("Modal bank");
    const double sr = 48000.0;
    {   // pitch and decay of one mode
        ModalBank bank; bank.prepare (sr);
        ModeTarget t; t.freq = 440.0; t.t60 = 1.0; t.in = 1.0f; t.outL = 1.0f; t.outR = 1.0f;
        std::vector<float> in (static_cast<size_t> (sr), 0.0f); in[0] = 1.0f;
        auto out = runBank (bank, t, in);
        CHECK_NEAR (zeroCrossingHz (out, 100, 24000, sr), 440.0, 0.3);
        const double a = rms (out, 4800, 9600), b = rms (out, 28800, 33600);    // 0.1 s -> 0.6 s
        CHECK_NEAR (20 * std::log10 (b / a), -30.0, 1.0);
    }
    {   // broadband input: each mode passes the same power whatever its decay
        for (double t60 : { 0.1, 1.0, 5.0 })
        {
            ModalBank bank; bank.prepare (sr);
            ModeTarget t; t.freq = 1000.0; t.t60 = t60; t.in = 1.0f; t.outL = 1.0f;
            Rng rng (7);
            std::vector<float> in (static_cast<size_t> (sr * 12)); for (auto& x : in) x = rng.bipolar() * 0.1f;
            auto out = runBank (bank, t, in);
            const double ratio = rms (out, static_cast<size_t> (sr * 6), in.size()) / rms (in, 0, in.size());
            const double r = std::exp (-6.907755 / (t60 * sr)), q = 1 - r * r;
            const double expect = std::sqrt (0.5) * std::pow (q, -0.15);            // Im part, with the impulse lean
            CHECK (ratio > expect * 0.8 && ratio < expect * 1.2);
        }
    }
    {   // a tone right on a long mode cannot run away: amplitude is capped, still a sinusoid
        ModalBank bank; bank.prepare (sr);
        ModeTarget t; t.freq = 440.0; t.t60 = 20.0; t.in = 1.0f; t.outL = 1.0f;
        std::vector<float> in (static_cast<size_t> (sr * 4));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * std::sin (2 * kPi * 440.0 * i / sr);
        auto out = runBank (bank, t, in);
        float mx = 0; for (float v : out) mx = std::max (mx, std::abs (v));
        CHECK (mx <= bank.limit * 1.0001f && mx > 0.9f);
        CHECK_NEAR (zeroCrossingHz (out, static_cast<size_t> (sr * 3), in.size(), sr), 440.0, 0.3);
    }
    {   // stability: 32 modes, targets jumping every sub-block, noise input, 20 s
        ModalBank bank; bank.prepare (sr);
        Rng rng (99);
        ModeTarget ts[kMaxModes];
        std::vector<float> in (kSubBlock), outL (kSubBlock), outR (kSubBlock);
        bool finite = true; float mx = 0;
        for (int blk = 0; blk < static_cast<int> (sr * 20 / kSubBlock); ++blk)
        {
            for (auto& t : ts) { t.freq = 20 + rng.uniform() * 30000; t.t60 = 0.01 + rng.uniform() * 30; t.in = rng.bipolar(); t.outL = rng.bipolar(); t.outR = rng.bipolar(); }
            bank.setTargets (ts, kMaxModes);
            for (auto& x : in) x = rng.bipolar();
            std::fill (outL.begin(), outL.end(), 0.0f); std::fill (outR.begin(), outR.end(), 0.0f);
            bank.process (in.data(), outL.data(), outR.data(), kSubBlock);
            for (int i = 0; i < kSubBlock; ++i) { finite &= std::isfinite (outL[i]) && std::isfinite (outR[i]); mx = std::max (mx, std::abs (outL[i])); }
        }
        CHECK (finite);
        CHECK (mx <= kMaxModes * bank.limit);
    }
}

// ------------------------------------------------------------------ engine
static std::vector<float> runEngine (NodalEngine& e, std::vector<float> in, int block, std::vector<float>* right = nullptr)
{
    std::vector<float> r = in;
    for (size_t pos = 0; pos < in.size(); pos += static_cast<size_t> (block))
    {
        const int n = static_cast<int> (std::min<size_t> (static_cast<size_t> (block), in.size() - pos));
        e.process (in.data() + pos, r.data() + pos, n);
    }
    if (right) *right = r;
    return in;
}

static void testEngine()
{
    std::puts ("Engine");
    const double sr = 48000.0;
    EngineParams p;

    {   // silence stays silent
        NodalEngine e; e.prepare (sr); e.setParams (p);
        auto out = runEngine (e, std::vector<float> (48000, 0.0f), 512);
        float mx = 0; for (float v : out) mx = std::max (mx, std::abs (v));
        CHECK (mx == 0.0f);
    }
    {   // lowest mode sits on the snapped body pitch
        EngineParams q = p; q.pitch = 45.3; q.key = 9; q.scale = 1; q.mix = 1.0; q.glideMs = 0;
        NodalEngine e; e.prepare (sr); e.setParams (q);
        std::vector<float> in (4800, 0.0f); in[10] = 1.0f;
        runEngine (e, in, 256);
        CHECK_NEAR (e.currentNote(), 45.0, 1e-9);
        CHECK_NEAR (e.modeFrequency (0), 110.0, 110.0 * 0.001);
        CHECK (e.activeModes() == q.density);
        const auto& en = e.modeEnergies(); float tot = 0; for (int i = 0; i < kMaxModes; ++i) tot += en[i];
        CHECK (tot > 0.0f);
    }
    {   // scale lock puts every mode on a scale note (within the beating detune)
        EngineParams q = p; q.tuneMode = TuneScale; q.lock = 1.0; q.scale = 8; q.key = 2; q.pitch = 50; q.glideMs = 0;
        NodalEngine e; e.prepare (sr); e.setParams (q);
        std::vector<float> in (512, 0.0f); runEngine (e, in, 512);
        for (int i = 0; i < q.density; ++i)
        {
            const double f = e.modeFrequency (i);
            if (f > sr * 0.45) continue;
            const double n = hzToNote (f);
            CHECK_NEAR (n, snapToScale (n, q.key, q.scale), 0.02);
        }
    }
    {   // the result does not depend on the host block size
        Rng rng (3); std::vector<float> in (48000); for (auto& x : in) x = rng.bipolar() * 0.3f;
        NodalEngine a, b; a.prepare (sr); b.prepare (sr); a.setParams (p); b.setParams (p);
        std::vector<float> ra, rb;
        auto la = runEngine (a, in, 64, &ra);
        auto lb = runEngine (b, in, 441, &rb);
        double diff = 0; for (size_t i = 0; i < la.size(); ++i) diff = std::max (diff, double (std::abs (la[i] - lb[i]) + std::abs (ra[i] - rb[i])));
        CHECK (diff < 1e-6);
    }
    {   // mix 0 is a clean bypass
        EngineParams q = p; q.mix = 0.0; q.outDb = 0.0;
        NodalEngine e; e.prepare (sr); e.setParams (q);
        Rng rng (5); std::vector<float> in (9600); for (auto& x : in) x = rng.bipolar() * 0.5f;
        auto out = runEngine (e, in, 300);
        double diff = 0; for (size_t i = 0; i < in.size(); ++i) diff = std::max (diff, double (std::abs (out[i] - in[i])));
        CHECK (diff < 1e-6);
    }
    {   // worst case: full-scale tones on the modes, max drive, long decay -> output stays bounded
        EngineParams q = p; q.mix = 1.0; q.driveDb = 24; q.decay = 20; q.damping = 0; q.density = 32; q.glideMs = 0;
        NodalEngine e; e.prepare (sr); e.setParams (q);
        std::vector<float> in (static_cast<size_t> (sr * 5));
        runEngine (e, std::vector<float> (512, 0.0f), 512);
        const double f0 = e.modeFrequency (0), f1 = e.modeFrequency (1);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * float (std::sin (2 * kPi * f0 * i / sr) + std::sin (2 * kPi * f1 * i / sr));
        std::vector<float> r;
        auto out = runEngine (e, in, 512, &r);
        float mx = 0; bool fin = true;
        for (size_t i = 0; i < out.size(); ++i) { mx = std::max ({ mx, std::abs (out[i]), std::abs (r[i]) }); fin &= std::isfinite (out[i]) && std::isfinite (r[i]); }
        CHECK (fin && mx <= 1.0f);
    }
    {   // useful levels: noise and a drum-like hit both come out audibly, stereo differs
        EngineParams q = p; q.mix = 1.0;
        NodalEngine e; e.prepare (sr); e.setParams (q);
        Rng rng (11); std::vector<float> in (static_cast<size_t> (sr * 3)); for (auto& x : in) x = rng.bipolar() * 0.25f;
        std::vector<float> r; auto l = runEngine (e, in, 512, &r);
        const double inDb = 20 * std::log10 (rms (in, 0, in.size())), outDb = 20 * std::log10 (rms (l, 48000, l.size()));
        std::printf ("    noise in %.1f dBFS -> wet L %.1f dBFS\n", inDb, outDb);
        CHECK (outDb > inDb - 9 && outDb < inDb + 3);
        double sd = 0; for (size_t i = 0; i < l.size(); ++i) sd += std::abs (l[i] - r[i]);
        CHECK (sd > 1.0);

        NodalEngine h; h.prepare (sr); h.setParams (q);
        std::vector<float> hit (static_cast<size_t> (sr * 2), 0.0f);
        Rng hr (21);   // snare-like: noise burst over a low thump, ~20 ms
        for (int i = 0; i < 2400; ++i) hit[static_cast<size_t> (i)] = float (std::exp (-i / 480.0) * (0.6 * hr.bipolar() + 0.6 * std::sin (2 * kPi * 180.0 * i / sr)));
        auto hl = runEngine (h, hit, 512);
        const double tail = 20 * std::log10 (rms (hl, 4800, 9600) + 1e-12);
        std::printf ("    drum-like hit -> ring at 0.1-0.2 s %.1f dBFS\n", tail);
        CHECK (tail > -36.0);
    }
}

int main()
{
    testBessel();
    testBodies();
    testTuning();
    testModalBank();
    testEngine();
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
