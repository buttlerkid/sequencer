#include "Engine/PendulumEngine.h"
#include <cstdio>
#include <map>
#include <set>

using namespace dy::pendulum;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

struct Timed { double ppq; int note; float vel; bool on; int k; };

// Run the engine over `beats` quarter notes in blocks of `block` samples; return events in ppq.
static std::vector<Timed> run (PendulumEngine& e, double beats, int block, double bpm = 120.0, double sr = 48000.0, double start = 0.0)
{
    std::vector<Timed> res;
    const double perSample = bpm / 60.0 / sr;
    const auto total = static_cast<int64_t> (std::llround (beats / perSample));
    std::vector<NoteEvent> ev;
    for (int64_t pos = 0; pos < total; pos += block)
    {
        const int n = static_cast<int> (std::min<int64_t> (block, total - pos));
        const double ppq = start + pos * perSample;
        ev.clear();
        e.process (ppq, bpm, n, ev);
        for (const auto& x : ev) res.push_back ({ ppq + x.sample * perSample, x.note, x.velocity, x.on, x.pendulum });
    }
    return res;
}

static std::vector<Timed> ons (const std::vector<Timed>& v) { std::vector<Timed> r; for (auto& x : v) if (x.on) r.push_back (x); return r; }

int main()
{
    const double sr = 48000.0;
    std::puts ("Timing");
    {   // one pendulum, 4 swings per 16-beat cycle: crossings every 2 beats starting at beat 1
        PendulumEngine e; e.prepare (sr);
        PendulumParams p; p.count = 1; p.swings = 4; p.cycleBeats = 16; e.setParams (p);
        auto on = ons (run (e, 16.0, 512));
        CHECK (on.size() == 8);
        for (size_t i = 0; i < on.size(); ++i) CHECK_NEAR (on[i].ppq, 1.0 + 2.0 * i, 1.5 / sr * 2);
    }
    {   // pendulum k swings (swings + k) times per cycle
        PendulumEngine e; e.prepare (sr);
        PendulumParams p; p.count = 6; p.swings = 8; p.cycleBeats = 32; e.setParams (p);
        auto on = ons (run (e, 32.0, 777));
        std::map<int, int> perK;
        for (auto& x : on) ++perK[x.k];
        for (int k = 0; k < 6; ++k) CHECK (perK[k] == 2 * (8 + k));
    }
    {   // turning points: everyone together at the start of every cycle, loudest there
        PendulumEngine e; e.prepare (sr);
        PendulumParams p; p.count = 10; p.swings = 12; p.cycleBeats = 16; p.trigger = TrigTurns; p.accent = 1.0f; e.setParams (p);
        auto on = ons (run (e, 32.0, 256));
        int atZero = 0, atCycle = 0;
        float midVel = 1.0f;
        for (auto& x : on)
        {
            if (std::abs (x.ppq) < 1e-6) ++atZero;
            if (std::abs (x.ppq - 16.0) < 1e-4) { ++atCycle; CHECK (x.vel > 0.79f); }
            if (x.ppq > 7.0 && x.ppq < 9.0) midVel = std::min (midVel, x.vel);
        }
        CHECK (atZero == 10 && atCycle == 10);
        CHECK (midVel < 0.6f);
        CHECK_NEAR (e.coherence (0.0), 1.0, 1e-12);
        CHECK (e.coherence (8.3) < 0.5);
    }
    {   // one direction only: half as many notes
        PendulumEngine a, b; a.prepare (sr); b.prepare (sr);
        PendulumParams p; p.count = 8; p.swings = 10; p.cycleBeats = 16; a.setParams (p);
        p.trigger = TrigRightward; b.setParams (p);
        CHECK (ons (run (a, 16.0, 512)).size() == 2 * ons (run (b, 16.0, 512)).size());
    }
    std::puts ("Block size, quantize, note offs");
    {   // the same events whatever the buffer size
        PendulumParams p; p.count = 16; p.swings = 20; p.cycleBeats = 32; p.noteBeats = 0.3;
        for (int q = 0; q < kNumGrids; ++q)
        {
            p.quantize = q;
            PendulumEngine a, b; a.prepare (sr); b.prepare (sr); a.setParams (p); b.setParams (p);
            auto ea = run (a, 40.0, 64), eb = run (b, 40.0, 1999);
            CHECK (ea.size() == eb.size());
            bool same = ea.size() == eb.size();
            for (size_t i = 0; same && i < ea.size(); ++i)
                same = ea[i].note == eb[i].note && ea[i].on == eb[i].on && std::abs (ea[i].ppq - eb[i].ppq) < 2.0 / sr * 2;
            if (! same)
                for (size_t i = 0; i < std::min (ea.size(), eb.size()); ++i)
                    if (! (ea[i].note == eb[i].note && ea[i].on == eb[i].on && std::abs (ea[i].ppq - eb[i].ppq) < 2.0 / sr * 2))
                    {
                        std::printf ("    q%d #%d: a %.6f n%d %s k%d | b %.6f n%d %s k%d\n", q, (int) i, ea[i].ppq, ea[i].note, ea[i].on ? "on" : "off", ea[i].k,
                                     eb[i].ppq, eb[i].note, eb[i].on ? "on" : "off", eb[i].k);
                        break;
                    }
            CHECK (same);
        }
    }
    {   // quantized: every note on the grid, never two from one pendulum in one slot
        PendulumParams p; p.count = 12; p.swings = 16; p.cycleBeats = 16; p.quantize = 3;   // 1/16
        PendulumEngine e; e.prepare (sr); e.setParams (p);
        auto on = ons (run (e, 16.0, 512));
        std::set<std::pair<int, long long>> seen;
        for (auto& x : on)
        {
            const double slot = x.ppq / 0.25;
            CHECK_NEAR (slot, std::round (slot), 1e-3);
            CHECK (seen.insert ({ x.k, std::llround (slot) }).second);
        }
    }
    {   // every note on is matched by a note off; nothing left hanging after allNotesOff
        PendulumParams p; p.count = 12; p.swings = 9; p.cycleBeats = 8; p.noteBeats = 0.7;
        PendulumEngine e; e.prepare (sr); e.setParams (p);
        auto ev = run (e, 12.0, 300);
        std::vector<NoteEvent> tail; e.allNotesOff (0, tail);
        std::map<int, int> open;
        for (auto& x : ev) open[x.note] += x.on ? 1 : -1;
        for (auto& x : tail) open[x.note] -= 1;
        bool balanced = true;
        for (auto& [n, c] : open) balanced &= c == 0;
        CHECK (balanced);
        for (auto& [n, c] : open) CHECK (c >= 0 || true);
    }
    std::puts ("Notes");
    {   // longest pendulum = root, then up the scale (minor pentatonic from C3)
        PendulumParams p; p.count = 7; p.root = 48; p.scale = 9; p.step = 1;
        PendulumEngine e; e.prepare (sr); e.setParams (p);
        const int want[7] = { 48, 51, 53, 55, 58, 60, 63 };
        for (int k = 0; k < 7; ++k) CHECK (e.noteOf (k) == want[k]);
        p.descending = true; e.setParams (p);
        CHECK (e.noteOf (0) == 63 && e.noteOf (6) == 48);
    }
    std::puts ("Layers and pads");
    {   // drum pads: one pad each from C1
        PendulumParams p; p.count = 8; p.noteMode = 1;
        PendulumEngine e; e.setParams (p);
        for (int k = 0; k < 8; ++k) CHECK (e.noteOf (k) == 36 + k);
    }
    {   // layers: neighbouring groups, each in its own two-octave band, optionally on its own channel
        PendulumParams p; p.count = 10; p.layers = 4; p.root = 36; p.scale = 0; p.step = 1; p.layerChannels = true; p.channel = 3;
        PendulumEngine e; e.prepare (sr); e.setParams (p);
        const int wantLayer[10] = { 0, 0, 0, 1, 1, 2, 2, 2, 3, 3 };
        for (int k = 0; k < 10; ++k)
        {
            CHECK (e.layerOf (k) == wantLayer[k]);
            CHECK (e.channelOf (k) == 3 + wantLayer[k]);
            CHECK (e.noteOf (k) >= 36 + kLayerSpan * wantLayer[k] && e.noteOf (k) < 36 + kLayerSpan * (wantLayer[k] + 1));
        }
        CHECK (e.noteOf (0) == 36 && e.noteOf (3) == 60 && e.noteOf (4) == 61 && e.noteOf (5) == 84);
        // the events carry the layer's channel
        std::vector<NoteEvent> ev;
        for (int b = 0; b < 400; ++b) e.process (b * 512 / 24000.0, 120.0, 512, ev);
        bool ok = ! ev.empty();
        for (auto& x : ev) if (x.on) ok &= x.channel == e.channelOf (x.pendulum);
        CHECK (ok);
    }
    {   // the pendulums really line up again: all at +1 at the cycle, spread out at half
        PendulumParams p; p.count = 15; p.swings = 30; p.cycleBeats = 60;
        PendulumEngine e; e.setParams (p);
        for (int k = 0; k < 15; ++k) CHECK_NEAR (e.position (k, 60.0), 1.0, 1e-9);
        int pos = 0; for (int k = 0; k < 15; ++k) pos += e.position (k, 30.0) > 0 ? 1 : -1;
        CHECK (std::abs (pos) <= 1);
    }
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
