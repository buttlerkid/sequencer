#include "Engine/DubEngine.h"
#include <cstdio>
#include <vector>

using namespace dy::dub;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

constexpr double kSr = 48000.0, kBpm = 120.0;

// Run `seconds` of a constant (DC) or supplied signal through the engine from ppq 0; returns L and R.
static void run (DubEngine& e, std::vector<float>& L, std::vector<float>& R, double seconds, float dc, bool running = true, int block = 256)
{
    const int total = static_cast<int> (seconds * kSr);
    L.assign (static_cast<size_t> (total), dc);
    R.assign (static_cast<size_t> (total), dc);
    const double perSample = kBpm / 60.0 / kSr;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        e.process (L.data() + pos, R.data() + pos, n, pos * perSample, kBpm, running);
    }
}

static float rms (const std::vector<float>& v, int from, int to)
{
    double s = 0; to = std::min (to, static_cast<int> (v.size()));
    for (int i = from; i < to; ++i) s += v[static_cast<size_t> (i)] * v[static_cast<size_t> (i)];
    return to > from ? static_cast<float> (std::sqrt (s / (to - from))) : 0.0f;
}

static GlobalParams dryParams()
{
    GlobalParams p;
    p.target[ModRevLevel] = 0.0f;
    p.target[ModDelayLevel] = 0.0f;
    return p;
}

int main()
{
    std::puts ("Envelope");
    {
        Env e { 0.1f, 0.2f, 0.5f, 0.0f };
        CHECK_NEAR (e.at (0.0f), 0.0f, 1e-6f);
        CHECK_NEAR (e.at (0.05f), 0.5f, 1e-4f);
        CHECK_NEAR (e.at (0.1f), 1.0f, 1e-4f);
        CHECK_NEAR (e.at (0.8f), 0.5f, 1e-4f);         // reached sustain at the start of the fall
        CHECK_NEAR (e.at (0.9f), 0.25f, 1e-4f);
        CHECK_NEAR (e.at (1.0f), 0.0f, 1e-6f);
        Env gate { 0.0f, 0.0f, 1.0f, 0.0f };
        CHECK_NEAR (gate.at (0.5f), 1.0f, 1e-6f);
        Env over { 0.8f, 0.8f, 1.0f, 0.0f };           // rise + fall > 100 % scale down together
        CHECK (over.at (0.5f) > 0.99f);
        Env bent { 1.0f, 0.0f, 1.0f, 1.0f };
        CHECK (bent.at (0.5f) < 0.2f);                  // positive curve starts slow
        Env fast { 1.0f, 0.0f, 1.0f, -1.0f };
        CHECK (fast.at (0.5f) > 0.8f);
    }

    std::puts ("Locate: steps, swing, patterns, chains");
    {
        DubEngine e; e.prepare (kSr);
        GlobalParams p; p.rate = 2; e.setParams (p);             // 1/16
        auto a = e.locate (0.0);
        CHECK (a.absStep == 0 && a.step == 0 && a.bank == 0);
        auto b = e.locate (0.25 * 5 + 0.01);
        CHECK (b.absStep == 5 && b.step == 5);
        auto c = e.locate (4.0 + 0.01);                           // bar 2, pattern A
        CHECK (c.step == 0 && c.bank == 0);

        p.pattern = 3; e.setParams (p);                           // AAB
        CHECK (e.locate (0.1).bank == 0);
        CHECK (e.locate (4.1).bank == 0);
        CHECK (e.locate (8.1).bank == 1);
        CHECK (e.locate (12.1).bank == 0);

        p.pattern = 0; p.length = 12; e.setParams (p);           // 12 steps: pass wraps at 3 beats
        CHECK (e.locate (3.01).step == 0);
        CHECK (e.locate (2.99).step == 11);

        p.length = 16; p.swing = 0.5f; e.setParams (p);          // second 1/16 starts 3/4 through the pair
        CHECK (e.locate (0.3).step == 0);
        CHECK (e.locate (0.4).step == 1);
        CHECK (e.locate (0.374).step == 0);

        p.swing = 0.0f; e.setParams (p);
        Banks banks {};
        for (int s = 0; s < 3; ++s) banks[0][static_cast<size_t> (s)].chain = true;   // steps 1-4 one block
        e.setBanks (banks);
        auto d = e.locate (0.25 * 2 + 0.125);                     // middle of step 3
        CHECK (d.blockStart == 0 && d.blockLen == 4);
        CHECK_NEAR (d.blockPhase, 2.5f / 4.0f, 1e-3f);
        CHECK (d.blockAbs == 0);
        auto f = e.locate (0.25 * 4 + 0.1);                       // step 5 is on its own
        CHECK (f.blockStart == 4 && f.blockLen == 1);
        auto neg = e.locate (-0.1);                               // pre-roll before the song start
        CHECK (neg.step == 15);
    }

    std::puts ("Probability");
    {
        DubEngine e; e.prepare (kSr);
        Step s; s.prob = 0.5f;
        int plays = 0;
        for (int64_t i = 0; i < 4000; ++i) plays += e.blockPlays (s, i, 0) ? 1 : 0;
        CHECK (plays > 1800 && plays < 2200);
        bool same = true;
        for (int64_t i = 0; i < 100; ++i) same = same && e.blockPlays (s, i, 0) == e.blockPlays (s, i, 0);
        CHECK (same);                                             // the same song position always decides the same
        s.on = false; s.prob = 1.0f;
        CHECK (! e.blockPlays (s, 3, 0));
    }

    std::puts ("Gating: off steps are silent, volume and pan per step");
    {
        DubEngine e; e.prepare (kSr);
        e.setParams (dryParams());
        Banks banks {};
        for (auto& st : banks[0]) st.venv = { 0.0f, 0.0f, 1.0f, 0.0f };
        banks[0][1].on = false;
        banks[0][2].vol = 0.5f;
        banks[0][3].pan = -1.0f;
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 0.5, 0.5f);                                 // 120 bpm: a 1/16 step is 6000 samples
        const int st = 6000;
        CHECK_NEAR (rms (L, 1000, 5000), 0.5f, 0.01f);
        CHECK (rms (L, st + 500, 2 * st - 500) < 1e-3f);
        CHECK_NEAR (rms (L, 2 * st + 500, 3 * st - 500), 0.25f, 0.01f);
        CHECK (rms (R, 3 * st + 500, 4 * st - 500) < 1e-3f);    // hard left
        CHECK_NEAR (rms (L, 3 * st + 500, 4 * st - 500), 0.5f * 1.414f, 0.02f);
    }

    std::puts ("Chain: the envelope stretches over the block");
    {
        DubEngine e; e.prepare (kSr);
        e.setParams (dryParams());
        Banks banks {};
        for (auto& st : banks[0]) st.venv = { 0.0f, 1.0f, 1.0f, 0.0f };        // a ramp down over the block
        for (int s = 0; s < 3; ++s) banks[0][static_cast<size_t> (s)].chain = true;
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 0.5, 1.0f);
        // halfway through the 4-step block (sample 12000) the ramp is at 0.5
        CHECK_NEAR (L[12000], 0.5f, 0.02f);
        CHECK_NEAR (L[6000], 0.75f, 0.02f);
        CHECK (L[3000] > 0.8f);                                   // no restart on step 2
    }

    std::puts ("Filter: stable and doing something");
    {
        DubEngine e; e.prepare (kSr);
        auto p = dryParams();
        e.setParams (p);
        Banks banks {};
        for (auto& st : banks[0]) { st.cut = 0.3f; st.res = 0.3f; st.fdepth = 5.0f; st.venv = { 0, 0, 1, 0 }; st.fenv = { 0.0f, 0.0f, 0.0f, 0.0f }; }
        e.setBanks (banks);
        std::vector<float> L, R;
        const int total = static_cast<int> (kSr * 2);
        L.resize (static_cast<size_t> (total)); R.resize (static_cast<size_t> (total));
        uint32_t rng = 1;
        for (int i = 0; i < total; ++i) { rng = rng * 1664525u + 1013904223u; L[static_cast<size_t> (i)] = R[static_cast<size_t> (i)] = ((rng >> 9) / 4194304.0f - 1.0f) * 0.5f; }
        const auto inRms = rms (L, 0, total);
        for (int pos = 0; pos < total; pos += 128) e.process (L.data() + pos, R.data() + pos, std::min (128, total - pos), pos * kBpm / 60.0 / kSr, kBpm, true);
        bool finite = true; float pk = 0;
        for (auto x : L) { finite = finite && std::isfinite (x); pk = std::max (pk, std::abs (x)); }
        CHECK (finite);
        CHECK (pk < 20.0f);
        CHECK (rms (L, 0, total) < inRms);                        // low-passed noise loses energy
    }

    std::puts ("Delay: an echo one 1/8 later, only from steps that send");
    {
        DubEngine e; e.prepare (kSr);
        auto p = dryParams();
        p.target[ModDelayLevel] = 1.0f; p.target[ModDelayFb] = 0.0f; p.delayTime = 3;       // 1/8 = 12000 samples
        p.pingPong = false; p.delayHpHz = 5.0f; p.delayLpHz = 20000.0f;
        e.setParams (p);
        Banks banks {};
        for (auto& st : banks[0]) { st.venv = { 0, 0, 1, 0 }; st.on = false; }
        banks[0][0].on = true; banks[0][0].dly = true;
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 1.0, 0.5f);
        CHECK (rms (L, 6500, 11500) < 1e-3f);                     // steps 2.. are off and the echo has not arrived
        CHECK (rms (L, 12500, 17500) > 0.2f);                     // the echo of step 1
        CHECK (rms (L, 25000, 47000) < 0.02f);                    // no feedback: one repeat only

        // high feedback at 100 % stays bounded
        p.target[ModDelayFb] = 1.0f; e.setParams (p);
        run (e, L, R, 4.0, 0.5f);
        float pk = 0; bool finite = true;
        for (auto x : L) { finite = finite && std::isfinite (x); pk = std::max (pk, std::abs (x)); }
        CHECK (finite);
        CHECK (pk < 4.0f);
    }

    std::puts ("Ping-pong: the first repeat is on the left, the second on the right");
    {
        DubEngine e; e.prepare (kSr);
        auto p = dryParams();
        p.target[ModDelayLevel] = 1.0f; p.target[ModDelayFb] = 0.8f; p.delayTime = 3;
        p.pingPong = true; p.delayHpHz = 5.0f; p.delayLpHz = 20000.0f;
        e.setParams (p);
        Banks banks {};
        for (auto& st : banks[0]) { st.venv = { 0, 0, 1, 0 }; st.on = false; }
        banks[0][0].on = true; banks[0][0].dly = true;
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 0.7, 0.5f);
        CHECK (rms (L, 12500, 17500) > 0.1f);
        CHECK (rms (R, 12500, 17500) < 0.01f);
        CHECK (rms (R, 24500, 29500) > 0.1f);
    }

    std::puts ("Reverbs: every type rings and decays");
    for (int type = 0; type < kNumReverbTypes; ++type)
    {
        DubEngine e; e.prepare (kSr);
        auto p = dryParams();
        p.target[ModRevLevel] = 1.0f; p.revType = type; p.target[ModMix] = 1.0f;
        e.setParams (p);
        Banks banks {};
        for (auto& st : banks[0]) { st.venv = { 0, 0, 1, 0 }; st.on = false; }
        banks[0][0].on = true; banks[0][0].rev = true;
        e.setBanks (banks);
        std::vector<float> L, R;
        // a short burst, then silence (input zero after the first step)
        const int total = static_cast<int> (kSr * 8);
        L.assign (static_cast<size_t> (total), 0.0f); R = L;
        for (int i = 0; i < 2000; ++i) L[static_cast<size_t> (i)] = R[static_cast<size_t> (i)] = (i % 40 < 20) ? 0.5f : -0.5f;
        for (int pos = 0; pos < total; pos += 256) e.process (L.data() + pos, R.data() + pos, std::min (256, total - pos), pos * kBpm / 60.0 / kSr, kBpm, true);
        const float early = rms (L, 6000, 30000), late = rms (L, total - 48000, total);
        bool finite = true; for (auto x : L) finite = finite && std::isfinite (x);
        std::printf ("    %-8s early %.4f  late %.6f\n", reverbName (type), early, late);
        CHECK (finite);
        CHECK (early > 0.002f);
        CHECK (late < early * 0.1f);
    }

    std::puts ("Modulation: synced LFO on level, follower ducks");
    {
        DubEngine e; e.prepare (kSr);
        auto p = dryParams();
        p.target[ModLevel] = 0.75f;
        p.lfo[0] = { 1.0, true, 4, ShapeSquare, ModLevel, 0.5f };   // 1 bar square: +0.5 of range then -0.5
        e.setParams (p);
        Banks banks {};
        for (auto& st : banks[0]) st.venv = { 0, 0, 1, 0 };
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 2.0, 0.5f);                                 // one bar = 2 s at 120 bpm
        CHECK_NEAR (rms (L, 4000, 40000), 0.75f, 0.02f);          // 0.5 * clamp(0.5 + 0.5) * 1.5
        CHECK (rms (L, 52000, 90000) < 0.01f);                    // 0.5 - 0.5 = 0
        CHECK_NEAR (e.telemetry.modNorm[ModLevel].load(), 1.0f, 1e-3f);    // the last block ends on bar 2: high again

        DubEngine f; f.prepare (kSr);
        auto q = dryParams();
        q.follower = { 1.0, 50.0, ModLevel, -0.6f };               // duck the level as the input gets louder
        f.setParams (q);
        f.setBanks (banks);
        run (f, L, R, 0.5, 0.5f);
        // input at -6 dB -> follower 0.9 -> level norm 0.667 - 0.54 = 0.127 -> x 1.5 = 0.19
        CHECK (rms (L, 10000, 20000) < 0.15f);
    }

    std::puts ("Stopped: the input passes through");
    {
        DubEngine e; e.prepare (kSr);
        e.setParams (dryParams());
        Banks banks {};
        for (auto& st : banks[0]) st.on = false;
        e.setBanks (banks);
        std::vector<float> L, R;
        run (e, L, R, 0.2, 0.3f, false);
        CHECK_NEAR (rms (L, 1000, 9000), 0.3f, 0.005f);
    }

    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
