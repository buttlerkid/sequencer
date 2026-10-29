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
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
