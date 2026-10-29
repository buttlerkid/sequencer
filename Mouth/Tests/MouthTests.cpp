#include "Engine/MouthMapper.h"
#include <cmath>
#include <cstdio>

using namespace dy::mouth;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

// A face: eyes 100 px apart, the mouth below; rotated by `angle` around (200, 200).
static std::vector<Point> face (float gap, float width, float lift, float angle = 0.0f, float brow = 0.3f)
{
    std::vector<Point> p (Mesh::kCount, Point { 200, 200 });
    auto put = [&] (int i, float x, float y)
    {
        const float c = std::cos (angle), s = std::sin (angle);
        p[(size_t) i] = { 200 + c * x - s * y, 200 + s * x + c * y };
    };
    put (Mesh::eyeR, -50, -40);  put (Mesh::eyeL, 50, -40);
    put (Mesh::lidR, -30, -45);  put (Mesh::lidL, 30, -45);
    put (Mesh::browR, -30, -45 - brow * 100); put (Mesh::browL, 30, -45 - brow * 100);
    put (Mesh::cornerR, -width * 50, 60); put (Mesh::cornerL, width * 50, 60);
    put (Mesh::lipTop, 0, 60 + lift * 100);
    put (Mesh::innerTop, 0, 62); put (Mesh::innerBottom, 0, 62 + gap * 100);
    put (Mesh::nose, 0, 10);
    return p;
}

static int count (const std::vector<MidiOut>& v, MidiOut::Type t) { int n = 0; for (auto& m : v) n += m.type == t; return n; }

// Faces with the given Open per face (-1 = not in view).
static Faces opens (std::initializer_list<float> o)
{
    Faces f; int i = 0;
    for (float v : o) { if (v >= 0) { f.present[(size_t) i] = true; f.shapes[(size_t) i][Open] = v; } ++i; }
    return f;
}

int main()
{
    std::puts ("Shape");
    {
        auto s = measure (face (0.3f, 0.6f, -0.1f).data());
        CHECK_NEAR (s.gap, 0.3f, 1e-4f);
        CHECK_NEAR (s.width, 0.6f, 1e-4f);
        CHECK_NEAR (s.lift, -0.1f, 1e-4f);
        CHECK_NEAR (s.brow, 0.3f, 1e-4f);
        CHECK_NEAR (s.roll, 0.0f, 1e-5f);
        // a tilted head measures the same, and knows it is tilted
        auto r = measure (face (0.3f, 0.6f, -0.1f, 0.4f).data());
        CHECK_NEAR (r.gap, 0.3f, 1e-4f);
        CHECK_NEAR (r.width, 0.6f, 1e-4f);
        CHECK_NEAR (r.lift, -0.1f, 1e-4f);
        CHECK_NEAR (r.brow, 0.3f, 1e-4f);
        CHECK_NEAR (r.roll, -0.4f, 1e-4f);
        // and so does a face twice as close
        auto big = face (0.3f, 0.6f, -0.1f);
        for (auto& q : big) q = { 2 * q.x, 2 * q.y };
        CHECK_NEAR (measure (big.data()).gap, 0.3f, 1e-4f);
        // the head in the frame, as the person sees it: mirrored, up = 1
        auto h = measure (face (0, 0.6f, 0).data(), 800, 420);
        CHECK_NEAR (h.headX, 0.75f, 1e-4f);
        CHECK_NEAR (h.headY, 0.5f, 1e-4f);
    }
    {   // calibration: the resting face is 0, the span is 1, clamped
        Calibration c;
        MouthShape rest { c.gapRest, c.widthRest, c.liftRest, c.browRest, c.rollRest };
        auto v = normalise (rest, c);
        for (int i : { Open, Width, Smile, Pucker, Brows }) CHECK_NEAR (v[(size_t) i], 0.0f, 1e-5f);
        CHECK_NEAR (v[Tilt], 0.5f, 1e-5f);
        MouthShape wide { c.gapRest + c.gapSpan, c.widthRest + c.widthSpan, c.liftRest + c.liftSpan, c.browRest + c.browSpan, c.rollSpan };
        v = normalise (wide, c);
        CHECK_NEAR (v[Open], 1.0f, 1e-5f); CHECK_NEAR (v[Width], 1.0f, 1e-5f); CHECK_NEAR (v[Smile], 1.0f, 1e-5f);
        CHECK_NEAR (v[Brows], 1.0f, 1e-5f); CHECK_NEAR (v[Tilt], 1.0f, 1e-5f);
        CHECK_NEAR (v[Pucker], 0.0f, 1e-5f);
        MouthShape oo { c.gapRest, c.widthRest - 2 * c.puckerSpan, c.liftRest };
        CHECK_NEAR (normalise (oo, c)[Pucker], 1.0f, 1e-5f);
    }

    std::puts ("Mapper");
    const double dt = 512.0 / 48000.0;
    {   // CC follows the smoothed value and is only sent when it changes
        MouthMapper m; MapperParams p; p.trigger = false;
        p.slots[0] = { Open, DestCC, 1, 50.0f, 0.0f, 1.0f, false, 0 };
        for (int i = 1; i < 4; ++i) p.slots[(size_t) i].dest = DestOff;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ 0 }), dt, out);
        CHECK (count (out, MidiOut::CC) == 1 && out[0].data1 == 1 && out[0].data2 == 0);
        out.clear(); m.process (opens ({ 0 }), dt, out);
        CHECK (out.empty());
        out.clear();
        for (int b = 0; b < 200; ++b) m.process (opens ({ 1 }), dt, out);
        CHECK (out.back().data2 == 127);
        bool rising = true; for (size_t i = 1; i < out.size(); ++i) rising &= out[i].data2 > out[i - 1].data2;
        CHECK (rising && out.size() > 5);        // a glide, not a jump
        // no face: hold
        out.clear(); m.process (opens ({ -1 }), dt, out);
        CHECK (out.empty());
    }
    {   // range, invert, the other destinations, and which face an output follows
        MouthMapper m; MapperParams p; p.trigger = false;
        p.slots[0] = { Width, DestPitchBend, 0, 1.0f, 0.5f, 1.0f, false, 0 };
        p.slots[1] = { Smile, DestPressure, 0, 1.0f, 0.0f, 1.0f, true, 0 };
        p.slots[2] = { Open, DestCC, 20, 1.0f, 0.0f, 1.0f, false, 1 };
        p.slots[3].dest = DestOff;
        p.channel = 3;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ 0, 1 }), dt, out);
        CHECK (out.size() == 3);
        CHECK (out[0].type == MidiOut::PitchBend && out[0].data1 == 8192 && out[0].channel == 3);
        CHECK (out[1].type == MidiOut::Pressure && out[1].data1 == 127);
        CHECK (out[2].type == MidiOut::CC && out[2].data1 == 20 && out[2].data2 == 127);   // face 2's open mouth
        // changing the CC number resends
        p.slots[1].dest = DestCC; p.slots[1].cc = 74; m.setParams (p);
        out.clear(); m.process (opens ({ 0, 1 }), dt, out);
        CHECK (out.size() == 1 && out[0].type == MidiOut::CC && out[0].data1 == 74 && out[0].data2 == 127);
    }
    {   // the note: on past the threshold, off below 70 % of it, off when the face goes
        MouthMapper m; MapperParams p;
        for (auto& s : p.slots) s.dest = DestOff;
        p.threshold = 0.3f; p.triggerNote = 50;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ 0.1f }), dt, out);
        m.process (opens ({ 0.29f }), dt, out);
        CHECK (out.empty());
        m.process (opens ({ 0.35f }), dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 1 && out[0].data1 == 50);
        m.process (opens ({ 0.25f }), dt, out);      // inside the hysteresis
        CHECK (count (out, MidiOut::NoteOff) == 0 && m.noteSounding (0));
        m.process (opens ({ 0.15f }), dt, out);
        CHECK (count (out, MidiOut::NoteOff) == 1 && ! m.noteSounding (0));
        out.clear();
        m.process (opens ({ 0.9f }), dt, out);
        m.process (opens ({ -1 }), dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 1 && count (out, MidiOut::NoteOff) == 1);
    }
    {   // a snap open plays louder than a slow one
        auto velocityAfter = [&] (int slowBlocks)
        {
            MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
            p.threshold = 0.4f; m.setParams (p);
            std::vector<MidiOut> out;
            m.process (opens ({ 0 }), dt, out);
            for (int b = 0; b < slowBlocks; ++b) m.process (opens ({ 0.3f }), dt, out);
            m.process (opens ({ 0.5f }), dt, out);
            return out.empty() ? -1 : out.back().data2;
        };
        const int fast = velocityAfter (0), slow = velocityAfter (30);
        CHECK (fast > 110 && slow < 60 && slow > 0);
    }

    std::puts ("Choir");
    {   // four faces share a major chord out
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.triggerNote = 48; p.chord = 1;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ 0.8f, 0.8f, 0.8f, 0.8f }), dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 4);
        CHECK (m.noteOf (0) == 48 && m.noteOf (1) == 52 && m.noteOf (2) == 55 && m.noteOf (3) == 60);
        // face 2 closes its mouth: only its note stops
        out.clear(); m.process (opens ({ 0.8f, 0.0f, 0.8f, 0.8f }), dt, out);
        CHECK (out.size() == 1 && out[0].type == MidiOut::NoteOff && out[0].data1 == 52);
    }
    {   // unison: two faces on one note do not cut each other off
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.chord = 0;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ 0.8f, 0.8f }), dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 1);
        out.clear(); m.process (opens ({ 0.0f, 0.8f }), dt, out);
        CHECK (out.empty());                                   // face 2 still holds it
        out.clear(); m.process (opens ({ 0.0f, 0.0f }), dt, out);
        CHECK (count (out, MidiOut::NoteOff) == 1);
    }
    {   // face = channel
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.chord = 0; p.channel = 2; p.faceChannels = true;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process (opens ({ -1, 0.8f, -1, 0.8f }), dt, out);
        CHECK (out.size() == 2 && out[0].channel == 3 && out[1].channel == 5);
    }
    {   // head height: the scale over two octaves, a little hysteresis, legato as the head moves
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.pitchMode = PitchHeight; p.scale = 2; p.triggerNote = 60;      // pentatonic: 11 notes, 60 .. 84
        m.setParams (p);
        auto at = [] (float y, float open) { Faces f; f.present[0] = true; f.shapes[0][Open] = open; f.shapes[0][HeadY] = y; return f; };
        std::vector<MidiOut> out;
        m.process (at (0.0f, 0.8f), dt, out);
        CHECK (m.noteOf (0) == 60);
        out.clear(); m.process (at (0.999f, 0.8f), dt, out);
        CHECK (m.noteOf (0) == 84);
        CHECK (out.size() == 2 && out[0].type == MidiOut::NoteOn && out[1].type == MidiOut::NoteOff && out[1].data1 == 60);
        // 4 / 11 is the step from G (67) to A (69): just past it stays on G, past the margin moves on
        m.process (at (3.5f / 11.0f, 0.8f), dt, out);
        CHECK (m.noteOf (0) == 67);
        m.process (at (4.1f / 11.0f, 0.8f), dt, out);
        CHECK (m.noteOf (0) == 67);
        m.process (at (4.4f / 11.0f, 0.8f), dt, out);
        CHECK (m.noteOf (0) == 69);
    }

    std::puts ("Auto range");
    {   // a face that only ever smiles a little still reaches 100 %, a twitch stays small
        Calibration c; AutoRange ar;
        MouthShape small { c.gapRest, c.widthRest, c.liftRest + 0.4f * c.liftSpan, c.browRest };
        const float before = normalise (small, c)[Smile];
        CHECK_NEAR (before, 0.4f, 1e-4f);
        CHECK_NEAR (normalise (small, c, &ar, 0.033f)[Smile], 1.0f, 1e-4f);
        MouthShape twitch { c.gapRest, c.widthRest, c.liftRest + 0.05f * c.liftSpan, c.browRest };
        CHECK (normalise (twitch, c, &ar, 0.033f)[Smile] < 0.2f);
        // the peak is forgotten slowly: after ten minutes it is down to the floor
        normalise (MouthShape { c.gapRest, c.widthRest, c.liftRest, c.browRest }, c, &ar, 600.0f);
        CHECK_NEAR (normalise (MouthShape { c.gapRest, c.widthRest, c.liftRest + 0.35f * c.liftSpan, c.browRest }, c, &ar, 0.0f)[Smile], 1.0f, 1e-3f);
    }

    std::puts ("Hands");
    {   // pinch and spread from the fingertips
        std::vector<Point> h (HandMesh::kCount, Point { 300, 300 });
        h[HandMesh::wrist] = { 300, 400 };
        for (int k : { HandMesh::indexKnuckle, HandMesh::middleKnuckle, HandMesh::ringKnuckle, HandMesh::pinkyKnuckle }) h[(size_t) k] = { 300, 300 };
        for (int k : { HandMesh::indexTip, HandMesh::middleTip, HandMesh::ringTip, HandMesh::pinkyTip }) h[(size_t) k] = { 300, 200 };   // open: 2 palms out
        h[HandMesh::thumbTip] = { 300, 200 };                                                                                          // touching the index
        auto s = measureHand (h.data(), 600, 800);
        CHECK (s.spread > 0.95f && s.pinch > 0.95f);
        CHECK_NEAR (s.x, 0.5f, 1e-4f);
        for (int k : { HandMesh::indexTip, HandMesh::middleTip, HandMesh::ringTip, HandMesh::pinkyTip }) h[(size_t) k] = { 300, 300 };   // a fist
        h[HandMesh::thumbTip] = { 420, 330 };
        s = measureHand (h.data(), 600, 800);
        CHECK (s.spread < 0.05f && s.pinch < 0.05f);
    }
    {   // circles: each full turn counts once, its size from the radius, a straight swipe never counts
        auto draw = [] (float radius, double turnsPerSecond, double seconds, int& count, CircleDetector::Event& last)
        {
            CircleDetector d; count = 0;
            for (double t = 0; t < seconds; t += 1.0 / 30.0)
            {
                const double a = 6.2831853 * turnsPerSecond * t;
                CircleDetector::Event e;
                if (d.add ({ 320 + radius * (float) std::cos (a), 240 + radius * (float) std::sin (a) }, t, 480, e)) { ++count; last = e; }
            }
            return d.circle();
        };
        int n = 0; CircleDetector::Event e {};
        const float level = draw (100, 1.0, 4.05, n, e);
        CHECK (n >= 3 && n <= 4);
        CHECK_NEAR (e.size, (100.0f / 480.0f - 0.03f) / 0.27f, 0.08f);
        CHECK (level > 0.4f);
        CHECK (e.direction == -1);                              // the image's clockwise is the person's anticlockwise
        int small = 0; CircleDetector::Event es {};
        draw (30, 2.0, 2.05, small, es);
        CHECK (small >= 3 && es.size < 0.15f && es.speed > 0.8f);
        CircleDetector d; int swipes = 0;
        for (double t = 0; t < 3; t += 1.0 / 30.0)
        {
            CircleDetector::Event x;
            swipes += d.add ({ 100 + (float) std::fmod (t * 400, 400), 240 }, t, 480, x) ? 1 : 0;
        }
        CHECK (swipes == 0);
    }
    {   // a circle plays a short note: bigger is lower, faster is louder
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.trigger = false; p.handNote = 36; p.scale = 2; p.handLength = 0.1f; p.handMode = HandCircles;
        m.setParams (p);
        CHECK (m.circlePitch (1.0f) == 36 && m.circlePitch (0.0f) == 60);
        Scene sc; std::vector<MidiOut> out;
        m.process (sc, dt, out);
        sc.circles[1] = 1; sc.circleSize[1] = 1.0f; sc.circleSpeed[1] = 1.0f;
        m.process (sc, dt, out);
        CHECK (out.size() == 1 && out[0].type == MidiOut::NoteOn && out[0].data1 == 36 && out[0].data2 == 127 && out[0].who == handWho (1));
        out.clear();
        for (int b = 0; b < 12; ++b) m.process (sc, dt, out);
        CHECK (out.size() == 1 && out[0].type == MidiOut::NoteOff && out[0].data1 == 36);
        // a hand as an output's source
        p.slots[0] = { Spin, DestCC, 9, 1.0f, 0.0f, 1.0f, false, handWho (0) };
        m.setParams (p);
        sc.present[(size_t) handWho (0)] = true; sc.shapes[(size_t) handWho (0)][Spin] = 1.0f;
        out.clear(); for (int b = 0; b < 50; ++b) m.process (sc, dt, out);
        CHECK (! out.empty() && out.back().data1 == 9 && out.back().data2 == 127);
    }

    std::puts ("Strums");
    {   // a quick swipe down is one strum, back up another; a slow drift or a sideways swipe none
        auto swipe = [] (StrumDetector& d, double& t, float y0, float y1, float x0, float x1, double seconds, int& n, StrumDetector::Event& last)
        {
            for (double k = 0; k <= seconds; k += 1.0 / 30.0, t += 1.0 / 30.0)
            {
                const float a = (float) (k / seconds);
                StrumDetector::Event e;
                if (d.add ({ x0 + (x1 - x0) * a, y0 + (y1 - y0) * a }, t, 480, e)) { ++n; last = e; }
            }
        };
        StrumDetector d; double t = 0; int n = 0; StrumDetector::Event e {};
        swipe (d, t, 100, 350, 300, 310, 0.2, n, e);          // down: 0.52 frame heights in 0.2 s
        CHECK (n == 1 && e.direction == 1 && e.speed > 0.2f);
        swipe (d, t, 350, 100, 310, 300, 0.2, n, e);          // and straight back up
        CHECK (n == 2 && e.direction == -1);
        int slow = 0; StrumDetector s2; double t2 = 0;
        swipe (s2, t2, 100, 350, 300, 300, 1.5, slow, e);     // a slow drift
        CHECK (slow == 0);
        int side = 0; StrumDetector s3; double t3 = 0;
        swipe (s3, t3, 200, 260, 50, 550, 0.2, side, e);      // a fast sideways swipe
        CHECK (side == 0);
        int longOne = 0; StrumDetector s4; double t4 = 0;
        swipe (s4, t4, 0, 470, 300, 300, 0.3, longOne, e);    // one long swipe is one strum
        CHECK (longOne == 1);
    }
    {   // air guitar: the strum rolls the chord the other hand's height picks, low string first going down
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.trigger = false; p.handMode = HandGuitar; p.handNote = 48; p.scale = 0; p.handLength = 0.3f;   // C major
        m.setParams (p);
        Scene sc; std::vector<MidiOut> out;
        const auto strummer = (size_t) handWho (0), fret = (size_t) handWho (1);
        sc.present[strummer] = sc.present[fret] = true;
        sc.shapes[strummer][HandX] = 0.4f; sc.shapes[fret][HandX] = 0.7f;
        sc.shapes[fret][HandY] = 4.5f / 7.0f;                    // the fifth degree: G major
        m.process (sc, dt, out);
        sc.strums[0] = 1; sc.strumSpeed[0] = 1.0f; sc.strumDirection[0] = 1;
        for (int b = 0; b < 6; ++b) m.process (sc, dt, out);
        std::vector<int> on; for (auto& x : out) if (x.type == MidiOut::NoteOn) on.push_back (x.data1);
        CHECK (on.size() == 4 && on[0] == 55 && on[1] == 59 && on[2] == 62 && on[3] == 67);
        out.clear();
        for (int b = 0; b < 40; ++b) m.process (sc, dt, out);
        CHECK (count (out, MidiOut::NoteOff) == 4);
        // an up strum: high string first
        out.clear();
        sc.strums[0] = 2; sc.strumDirection[0] = -1;
        for (int b = 0; b < 6; ++b) m.process (sc, dt, out);
        on.clear(); for (auto& x : out) if (x.type == MidiOut::NoteOn) on.push_back (x.data1);
        CHECK (on.size() == 4 && on[0] == 67 && on[3] == 55);
    }
    {   // theremin: an open hand sings its height, gliding; a fist stops it
        MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
        p.trigger = false; p.handMode = HandTheremin; p.handNote = 60; p.scale = 2;   // pentatonic, 11 steps
        m.setParams (p);
        Scene sc; std::vector<MidiOut> out;
        const auto w = (size_t) handWho (2);
        sc.present[w] = true; sc.shapes[w][Spread] = 0.2f; sc.shapes[w][HandY] = 0.0f;
        m.process (sc, dt, out);
        CHECK (out.empty() && m.handNoteOf (2) < 0);
        sc.shapes[w][Spread] = 0.9f;
        m.process (sc, dt, out);
        CHECK (m.handNoteOf (2) == 60 && count (out, MidiOut::NoteOn) == 1);
        sc.shapes[w][HandY] = 0.999f;
        out.clear(); m.process (sc, dt, out);
        CHECK (m.handNoteOf (2) == 84 && out.size() == 2 && out[0].type == MidiOut::NoteOn && out[1].type == MidiOut::NoteOff);
        sc.shapes[w][Spread] = 0.45f;                              // half closed: still singing
        out.clear(); m.process (sc, dt, out);
        CHECK (out.empty());
        sc.shapes[w][Spread] = 0.1f;
        m.process (sc, dt, out);
        CHECK (count (out, MidiOut::NoteOff) == 1 && m.handNoteOf (2) < 0);
    }
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
