// Engine unit tests. No framework: a tiny CHECK macro keeps the test binary
// dependency-free and fast to build (no JUCE).
#include "../Source/Engine/Euclid.h"
#include "../Source/Engine/Scale.h"
#include "../Source/Engine/Shuffle.h"
#include "../Source/Engine/Sequencer.h"
#include "../Source/Engine/Generator.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        ++g_checks;                                                                        \
        if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

using namespace dy;

static int popcount64 (uint64_t v) { int n = 0; while (v) { n += static_cast<int> (v & 1u); v >>= 1; } return n; }

static std::string maskString (uint64_t m, int steps)
{
    std::string s;
    for (int i = 0; i < steps; ++i) s += euclidHit (m, i) ? 'x' : '.';
    return s;
}

// ---------------------------------------------------------------------------
static void testEuclid()
{
    std::puts ("Euclid");
    CHECK (euclidean (8, 0, 0) == 0);
    CHECK (popcount64 (euclidean (8, 8, 0)) == 8);
    CHECK (popcount64 (euclidean (64, 64, 0)) == 64);
    CHECK (maskString (euclidean (8, 3, 0), 8) == "x..x..x.");   // tresillo
    CHECK (maskString (euclidean (8, 5, 0), 8) == "x.x.xx.x");   // cinquillo (rotation of Bjorklund)
    CHECK (maskString (euclidean (16, 4, 0), 16) == "x...x...x...x...");

    for (int steps = 1; steps <= 64; steps += 7)
        for (int pulses = 0; pulses <= steps; pulses += 3)
            for (int rot = 0; rot < steps; rot += 5)
                CHECK (popcount64 (euclidean (steps, pulses, rot)) == pulses);

    CHECK (maskString (euclidean (8, 3, 1), 8) == ".x..x..x");
    CHECK (euclidean (8, 3, 8) == euclidean (8, 3, 0));
    CHECK (euclidean (8, 3, -1) == euclidean (8, 3, 7));
}

// ---------------------------------------------------------------------------
static void testScale()
{
    std::puts ("Scale");
    CHECK (noteForDegree (0, 0, 48, 0) == 48);   // C3
    CHECK (noteForDegree (0, 0, 48, 2) == 52);   // E3
    CHECK (noteForDegree (0, 0, 48, 7) == 60);   // C4 (octave wrap)
    CHECK (noteForDegree (0, 0, 48, -1) == 47);  // B2
    CHECK (noteForDegree (0, 0, 48, -7) == 36);  // C2
    CHECK (noteForDegree (2, 1, 48, 2) == 53);   // D minor: D, E, F -> F3
    CHECK (noteForDegree (0, 9, 48, 5) == 60);   // major pentatonic wraps at 5 degrees
    CHECK (noteForDegree (0, 0, 48, 100) == 127);
    CHECK (noteForDegree (0, 0, 48, -100) == 0);

    CHECK (quantizeToScale (48, 0, 0) == 48);
    CHECK (quantizeToScale (49, 0, 0) == 48);    // C# -> C (tie resolves down)
    CHECK (quantizeToScale (51, 0, 0) == 50);    // D# -> D
    CHECK (quantizeToScale (61, 0, 0) == 60);
    CHECK (quantizeToScale (49, 1, 0) == 49);    // in C# major, C# is in scale
    CHECK (kNumScales >= 7);
}

// ---------------------------------------------------------------------------
static void testShuffle()
{
    std::puts ("Shuffle");
    CHECK (kNumShuffleProfiles == 8);
    CHECK_NEAR (swingOffsetSteps (0, 100, 0), 0.0, 1e-9);
    CHECK_NEAR (swingOffsetSteps (0, 100, 1), 0.5, 1e-9);
    CHECK_NEAR (swingOffsetSteps (0, 50, 1), 0.25, 1e-9);
    CHECK_NEAR (swingOffsetSteps (0, 0, 1), 0.0, 1e-9);
    CHECK_NEAR (swingOffsetSteps (0, 100, 17), 0.5, 1e-9);   // cycles every 16
    CHECK_NEAR (swingOffsetSteps (3, 100, 1), -0.25, 1e-9);  // Push profile is early
    for (int p = 0; p < kNumShuffleProfiles; ++p)
        CHECK_NEAR (swingOffsetSteps (p, 100, 0), 0.0, 1e-9); // downbeat never moves
}

// ---------------------------------------------------------------------------
struct Rig
{
    GlobalSettings g;
    TrackSettingsArray ts {};
    PatternModel pat;
    Sequencer seq;
    std::vector<MidiEvent> collected;
    double ppq = 0.0;

    Rig()
    {
        g.bpm = 120.0;
        g.sampleRate = 44100.0;
        ts[0].enabled = true;
        ts[0].steps = 16;
        ts[0].division = 0.25;
        ts[0].euclidMode = EuclidOff;
    }

    // Runs `bars` bars of 4/4 in `block`-sample chunks from the current ppq.
    void run (double bars, int block = 512)
    {
        std::vector<MidiEvent> out;
        const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
        const double end = ppq + bars * 4.0;
        while (ppq < end - 1e-9)
        {
            // Last block is trimmed so the run ends exactly on the boundary.
            const int n = static_cast<int> (std::min<long long> (block, std::llround ((end - ppq) / ppqPerSample)));
            if (n <= 0) break;
            Transport t { true, ppq, n };
            seq.process (t, g, ts, pat, out);
            for (auto& e : out)
            {
                e.sampleOffset = static_cast<int> (std::llround ((e.ppq - ppq) / ppqPerSample));
                collected.push_back (e);
            }
            ppq = (n == block) ? ppq + n * ppqPerSample : end;
        }
    }

    void stop()
    {
        std::vector<MidiEvent> out;
        Transport t { false, ppq, 512 };
        seq.process (t, g, ts, pat, out);
        for (auto& e : out) collected.push_back (e);
    }

    int countOns (int track = -1) const
    {
        int n = 0;
        for (const auto& e : collected) if (e.noteOn && (track < 0 || e.channel == ts[track].channel)) ++n;
        return n;
    }
    int countOffs() const { int n = 0; for (const auto& e : collected) if (! e.noteOn) ++n; return n; }

    std::vector<double> onTimes() const
    {
        std::vector<double> v;
        for (const auto& e : collected) if (e.noteOn) v.push_back (e.ppq);
        return v;
    }
};

static bool hasOnNear (const Rig& r, double ppq, double eps = 1e-6)
{
    for (const auto& e : r.collected) if (e.noteOn && std::abs (e.ppq - ppq) <= eps) return true;
    return false;
}

static void testSequencerBasics()
{
    std::puts ("Sequencer basics");
    Rig r;
    r.pat.tracks[0].setActive (0, true);
    r.pat.tracks[0].setActive (4, true);
    r.run (2.0);

    CHECK (r.countOns() == 4);
    CHECK (r.countOffs() >= 3);
    CHECK (hasOnNear (r, 0.0));
    CHECK (hasOnNear (r, 1.0));
    CHECK (hasOnNear (r, 4.0));
    CHECK (hasOnNear (r, 5.0));
    for (const auto& e : r.collected)
    {
        if (e.noteOn) { CHECK (e.note == 48); CHECK (e.velocity == 100); CHECK (e.channel == 1); }
        CHECK (e.sampleOffset >= 0 && e.sampleOffset < 512);
    }

    // Default length 80% of a 1/16 = 0.2 ppq
    bool sawOff = false;
    for (const auto& e : r.collected)
        if (! e.noteOn && std::abs (e.ppq - 0.2) < 1e-6) sawOff = true;
    CHECK (sawOff);

    // Stopping releases held notes and every on has an off.
    r.stop();
    CHECK (r.countOns() == r.countOffs());
    CHECK (r.seq.currentStep (0) == -1);
}

static void testSequencerEuclidAndPolymeter()
{
    std::puts ("Sequencer Euclid + polymeter");
    {
        Rig r;
        r.ts[0].euclidMode = EuclidOnly;
        r.ts[0].pulses = 4;
        r.run (1.0);
        CHECK (r.countOns() == 4);
        CHECK (hasOnNear (r, 0.0) && hasOnNear (r, 1.0) && hasOnNear (r, 2.0) && hasOnNear (r, 3.0));
    }
    {
        Rig r;
        r.ts[0].euclidMode = EuclidAdd;
        r.ts[0].pulses = 4;
        r.pat.tracks[0].setActive (2, true);   // manual hit layered on Euclid
        r.run (1.0);
        CHECK (r.countOns() == 5);
        CHECK (hasOnNear (r, 0.5));
    }
    {
        Rig r;
        r.ts[0].steps = 3;                     // 3-step loop against a 4/4 bar
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        CHECK (r.countOns() == 6);             // 16 sixteenths / 3 -> hits at k = 0,3,6,9,12,15
        CHECK (hasOnNear (r, 0.75) && hasOnNear (r, 3.75));
    }
    {
        Rig r;
        r.ts[0].mute = true;
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        CHECK (r.countOns() == 0);
    }
}

static void testSequencerLanes()
{
    std::puts ("Sequencer lanes");
    {   // negative micro-timing still arrives (lookahead)
        Rig r;
        r.pat.tracks[0].setActive (4, true);
        r.pat.tracks[0].set (Lane::Timing, 4, -64);
        r.run (1.0);
        CHECK (r.countOns() == 1);
        CHECK (hasOnNear (r, 1.0 - 0.128, 1e-6));   // 64ms at 120bpm = 0.128 ppq
    }
    {   // positive timing
        Rig r;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].set (Lane::Timing, 0, 32);
        r.run (1.0);
        CHECK (hasOnNear (r, 0.064));
    }
    {   // repeats subdivide the step
        Rig r;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].set (Lane::Repeats, 0, 4);
        r.run (1.0);
        CHECK (r.countOns() == 4);
        CHECK (hasOnNear (r, 0.0) && hasOnNear (r, 0.0625) && hasOnNear (r, 0.125) && hasOnNear (r, 0.1875));
    }
    {   // probability 0 never fires; probability 100 always fires
        Rig r;
        for (int i = 0; i < 16; ++i) { r.pat.tracks[0].setActive (i, true); r.pat.tracks[0].set (Lane::Probability, i, 0); }
        r.run (2.0);
        CHECK (r.countOns() == 0);
        Rig r2;
        for (int i = 0; i < 16; ++i) r2.pat.tracks[0].setActive (i, true);
        r2.run (2.0);
        CHECK (r2.countOns() == 32);
    }
    {   // ~50% probability lands in a sane band over many steps
        Rig r;
        for (int i = 0; i < 16; ++i) { r.pat.tracks[0].setActive (i, true); r.pat.tracks[0].set (Lane::Probability, i, 50); }
        r.run (32.0);
        const int n = r.countOns();
        CHECK (n > 512 * 0.35 && n < 512 * 0.65);
    }
    {   // interval + transpose select scale degrees; fixed pitch ignores them
        Rig r;
        r.g.key = 2; r.g.scale = 1;            // D minor
        r.ts[0].transpose = 1;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].set (Lane::Interval, 0, 1);   // degree 2 -> F3 = 53
        r.run (1.0);
        CHECK (r.collected.size() >= 1 && r.collected[0].note == 53);

        Rig f;
        f.ts[0].pitchMode = PitchFixed;
        f.ts[0].fixedNote = 37;
        f.ts[0].transpose = 5;
        f.pat.tracks[0].setActive (0, true);
        f.pat.tracks[0].set (Lane::Interval, 0, 3);
        f.run (1.0);
        CHECK (f.collected.size() >= 1 && f.collected[0].note == 37);
    }
    {   // velocity and length
        Rig r;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].set (Lane::Velocity, 0, 33);
        r.pat.tracks[0].set (Lane::Length, 0, 200);
        r.run (1.0);
        CHECK (r.collected[0].noteOn && r.collected[0].velocity == 33);
        bool off = false;
        for (const auto& e : r.collected) if (! e.noteOn && std::abs (e.ppq - 0.5) < 1e-6) off = true;
        CHECK (off);
    }
}

static void testSequencerSwing()
{
    std::puts ("Sequencer swing");
    {
        Rig r;
        r.g.swingProfile = 0; r.g.swingAmount = 100;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].setActive (1, true);
        r.run (1.0);
        CHECK (hasOnNear (r, 0.0));
        CHECK (hasOnNear (r, 0.25 + 0.125));   // off-step delayed half a step
    }
    {
        Rig r;
        r.g.swingAmount = 100;
        r.ts[0].swingMode = SwingOff;
        r.pat.tracks[0].setActive (1, true);
        r.run (1.0);
        CHECK (hasOnNear (r, 0.25));
    }
    {
        Rig r;
        r.ts[0].swingMode = SwingCustom;
        r.ts[0].swingProfile = 3;             // Push: -0.5 curve -> quarter step early at 100%
        r.ts[0].swingAmount = 100;
        r.pat.tracks[0].setActive (1, true);
        r.run (1.0);
        CHECK (hasOnNear (r, 0.25 - 0.0625));
    }
}

static void testSequencerTransport()
{
    std::puts ("Sequencer transport");
    {   // block size does not change results
        Rig a, b;
        for (Rig* r : { &a, &b }) { r->ts[0].pulses = 5; r->ts[0].euclidMode = EuclidOnly; r->pat.tracks[0].set (Lane::Timing, 3, -40); }
        a.run (4.0, 64);
        b.run (4.0, 2048);
        CHECK (a.countOns() == b.countOns());
        auto ta = a.onTimes(), tb = b.onTimes();
        CHECK (ta.size() == tb.size());
        for (size_t i = 0; i < ta.size() && i < tb.size(); ++i) CHECK_NEAR (ta[i], tb[i], 1e-9);
    }
    {   // a jump forward releases held notes and does not replay skipped steps
        Rig r;
        r.pat.tracks[0].set (Lane::Length, 0, 200);
        r.pat.tracks[0].setActive (0, true);
        r.run (0.125);                         // half a beat: note from step 0 still held
        r.ppq = 8.0;                           // jump to bar 3
        r.run (1.0);
        CHECK (r.countOns() == 2);             // step 0 at ppq 0 and at ppq 8, nothing in between
        CHECK (r.countOffs() == 2);
        CHECK (hasOnNear (r, 8.0));
    }
    {   // loop wrap backwards
        Rig r;
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        r.ppq = 0.0;
        r.run (1.0);
        CHECK (r.countOns() == 2);
    }
    {   // disabled track emits nothing, enabling mid-stream works
        Rig r;
        r.ts[0].enabled = false;
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        CHECK (r.countOns() == 0);
        r.ts[0].enabled = true;
        r.run (1.0);
        CHECK (r.countOns() == 1);
        CHECK (hasOnNear (r, 4.0));
    }
    {   // 16 tracks at once on distinct channels
        Rig r;
        for (int i = 0; i < kNumTracks; ++i)
        {
            r.ts[i].enabled = true; r.ts[i].channel = i + 1; r.ts[i].euclidMode = EuclidOnly; r.ts[i].pulses = 1 + i % 4;
        }
        r.run (1.0);
        for (int i = 0; i < kNumTracks; ++i) CHECK (r.countOns (i) == 1 + i % 4);
    }
    {   // current step tracks position
        Rig r;
        r.run (0.5);   // 2 beats = 8 sixteenths
        CHECK (r.seq.currentStep (0) >= 7 && r.seq.currentStep (0) <= 8);
    }
}


static void testMacros()
{
    std::puts ("Track macros + shift");
    {   // velocity offset and clamp
        Rig r;
        r.ts[0].velOffset = 40;
        r.pat.tracks[0].setActive (0, true);
        r.pat.tracks[0].set (Lane::Velocity, 0, 100);
        r.run (1.0);
        CHECK (r.collected[0].noteOn && r.collected[0].velocity == 127);
        Rig q;
        q.ts[0].velOffset = -64;
        q.pat.tracks[0].setActive (0, true);
        q.pat.tracks[0].set (Lane::Velocity, 0, 10);
        q.run (1.0);
        CHECK (q.collected[0].noteOn && q.collected[0].velocity == 1);
    }
    {   // length scale doubles the gate: 80% * 200% = 160% of a 1/16 = 0.4 ppq
        Rig r;
        r.ts[0].lengthScale = 200;
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        bool off = false;
        for (const auto& e : r.collected) if (! e.noteOn && std::abs (e.ppq - 0.4) < 1e-6) off = true;
        CHECK (off);
    }
    {   // probability scale 0 silences, 100 leaves alone
        Rig r;
        r.ts[0].probScale = 0;
        for (int i = 0; i < 16; ++i) r.pat.tracks[0].setActive (i, true);
        r.run (1.0);
        CHECK (r.countOns() == 0);
    }
    {   // repeats add
        Rig r;
        r.ts[0].repsAdd = 3;
        r.pat.tracks[0].setActive (0, true);
        r.run (1.0);
        CHECK (r.countOns() == 4);
    }
    {   // track shift + master shift + step timing all add up
        Rig r;
        r.ts[0].shiftMs = -30.0;
        r.g.masterShiftMs = -20.0;
        r.pat.tracks[0].setActive (4, true);
        r.pat.tracks[0].set (Lane::Timing, 4, -14);
        r.run (1.0);
        CHECK (r.countOns() == 1);
        CHECK (hasOnNear (r, 1.0 - 64.0 * 0.002, 1e-6));   // -64ms total at 120bpm
        Rig q;
        q.ts[0].shiftMs = 64.0;
        q.g.masterShiftMs = 64.0;
        q.pat.tracks[0].setActive (0, true);
        q.pat.tracks[0].set (Lane::Timing, 0, 64);
        q.run (1.0);
        CHECK (hasOnNear (q, 192.0 * 0.002, 1e-6));          // +192ms lookahead still lands
    }
    {   // negative shift on the very first step is dropped at transport start (in the past), later loops fire
        Rig r;
        r.ts[0].shiftMs = -64.0;
        r.pat.tracks[0].setActive (0, true);
        r.run (2.0);
        CHECK (r.countOns() == 2);                          // ppq 0 dropped; 4.0 and 8.0 pulled earlier
        CHECK (hasOnNear (r, 4.0 - 0.128) && hasOnNear (r, 8.0 - 0.128));
    }
}

static void testOfflineRender()
{
    std::puts ("Offline render");
    GlobalSettings g;
    TrackSettingsArray ts {};
    PatternModel pat;
    ts[0].enabled = true; ts[0].pulses = 4; ts[0].euclidMode = EuclidOnly;
    ts[1].enabled = true; ts[1].channel = 2; ts[1].steps = 12; ts[1].pulses = 5; ts[1].euclidMode = EuclidOnly;
    pat.tracks[1].set (Lane::Length, 0, 200);

    auto ev = Sequencer::renderOffline (g, ts, pat, 2);
    int ons = 0, offs = 0;
    for (const auto& e : ev) { if (e.noteOn) ++ons; else ++offs; CHECK (e.ppq >= 0.0 && e.ppq <= 8.0 + 1e-9); }
    CHECK (ons == offs);
    CHECK (ons == 8 + 13);                    // t0: 4/bar; t1: E(5,12) over 32 steps -> 13 hits

    // deterministic
    auto ev2 = Sequencer::renderOffline (g, ts, pat, 2);
    CHECK (ev.size() == ev2.size());
    for (size_t i = 0; i < ev.size() && i < ev2.size(); ++i) CHECK_NEAR (ev[i].ppq, ev2[i].ppq, 1e-12);
}

static void testGenerator()
{
    std::puts ("Generator");
    TrackModel t;
    Generator gen (42);
    gen.randomizeSteps (t, 16, 100);
    int n = 0; for (int i = 0; i < 16; ++i) n += t.isActive (i);
    CHECK (n == 16);
    gen.randomizeSteps (t, 16, 0);
    n = 0; for (int i = 0; i < 64; ++i) n += t.isActive (i);
    CHECK (n == 0);

    gen.arpeggiate (t, 8, ArpMode::Up, 2);
    CHECK (t.get (Lane::Interval, 0) == 0 && t.get (Lane::Interval, 1) == 2 && t.get (Lane::Interval, 2) == 4);
    CHECK (t.get (Lane::Interval, 3) == 7 && t.get (Lane::Interval, 6) == 0);
    gen.arpeggiate (t, 8, ArpMode::Down, 1);
    CHECK (t.get (Lane::Interval, 0) == 4 && t.get (Lane::Interval, 2) == 0 && t.get (Lane::Interval, 3) == 4);
    gen.arpeggiate (t, 8, ArpMode::UpDown, 1);
    CHECK (t.get (Lane::Interval, 0) == 0 && t.get (Lane::Interval, 1) == 2 && t.get (Lane::Interval, 2) == 4
           && t.get (Lane::Interval, 3) == 2 && t.get (Lane::Interval, 4) == 0);

    gen.randomizeLane (t, Lane::Velocity, 16, 40, 60);
    for (int i = 0; i < 16; ++i) CHECK (t.get (Lane::Velocity, i) >= 40 && t.get (Lane::Velocity, i) <= 60);

    // snapshot / load round-trip and clamping
    t.set (Lane::Velocity, 0, 999);
    CHECK (t.get (Lane::Velocity, 0) == 127);
    auto snap = t.snapshot();
    TrackModel u;
    u.load (snap);
    for (int i = 0; i < 64; ++i)
    {
        CHECK (u.isActive (i) == t.isActive (i));
        for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
            CHECK (u.get (static_cast<Lane> (l), i) == t.get (static_cast<Lane> (l), i));
    }
}

int main()
{
    testEuclid();
    testScale();
    testShuffle();
    testSequencerBasics();
    testSequencerEuclidAndPolymeter();
    testSequencerLanes();
    testSequencerSwing();
    testSequencerTransport();
    testMacros();
    testOfflineRender();
    testGenerator();

    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
