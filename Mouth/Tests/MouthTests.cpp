#include "Engine/MouthMapper.h"
#include <cmath>
#include <cstdio>

using namespace dy::mouth;

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (! (cond)) { ++g_failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

// A face: eyes 100 px apart, the mouth below; rotated by `angle` around the nose.
static std::vector<Point> face (float gap, float width, float lift, float angle = 0.0f)
{
    std::vector<Point> p (Mesh::kCount, Point { 200, 200 });
    auto put = [&] (int i, float x, float y)
    {
        const float c = std::cos (angle), s = std::sin (angle);
        p[(size_t) i] = { 200 + c * x - s * y, 200 + s * x + c * y };
    };
    put (Mesh::eyeR, -50, -40);  put (Mesh::eyeL, 50, -40);
    put (Mesh::cornerR, -width * 50, 60); put (Mesh::cornerL, width * 50, 60);
    put (Mesh::lipTop, 0, 60 + lift * 100);
    put (Mesh::innerTop, 0, 62); put (Mesh::innerBottom, 0, 62 + gap * 100);
    return p;
}

static int count (const std::vector<MidiOut>& v, MidiOut::Type t) { int n = 0; for (auto& m : v) n += m.type == t; return n; }

int main()
{
    std::puts ("Shape");
    {
        auto s = measure (face (0.3f, 0.6f, -0.1f).data());
        CHECK_NEAR (s.gap, 0.3f, 1e-4f);
        CHECK_NEAR (s.width, 0.6f, 1e-4f);
        CHECK_NEAR (s.lift, -0.1f, 1e-4f);
        // a tilted head measures the same
        auto r = measure (face (0.3f, 0.6f, -0.1f, 0.4f).data());
        CHECK_NEAR (r.gap, 0.3f, 1e-4f);
        CHECK_NEAR (r.width, 0.6f, 1e-4f);
        CHECK_NEAR (r.lift, -0.1f, 1e-4f);
        // and so does a face twice as close
        auto big = face (0.3f, 0.6f, -0.1f);
        for (auto& q : big) q = { 2 * q.x, 2 * q.y };
        CHECK_NEAR (measure (big.data()).gap, 0.3f, 1e-4f);
    }
    {   // calibration: the resting face is 0, the span is 1, clamped
        Calibration c;
        MouthShape rest { c.gapRest, c.widthRest, c.liftRest };
        auto v = normalise (rest, c);
        for (float x : v) CHECK_NEAR (x, 0.0f, 1e-5f);
        MouthShape wide { c.gapRest + c.gapSpan, c.widthRest + c.widthSpan, c.liftRest + c.liftSpan };
        v = normalise (wide, c);
        CHECK_NEAR (v[Open], 1.0f, 1e-5f); CHECK_NEAR (v[Width], 1.0f, 1e-5f); CHECK_NEAR (v[Smile], 1.0f, 1e-5f);
        CHECK_NEAR (v[Pucker], 0.0f, 1e-5f);
        MouthShape oo { c.gapRest, c.widthRest - 2 * c.puckerSpan, c.liftRest };
        CHECK_NEAR (normalise (oo, c)[Pucker], 1.0f, 1e-5f);
    }

    std::puts ("Mapper");
    const double dt = 512.0 / 48000.0;
    {   // CC follows the smoothed value and is only sent when it changes
        MouthMapper m; MapperParams p; p.trigger = false;
        p.slots[0] = { Open, DestCC, 1, 50.0f, 0.0f, 1.0f, false };
        for (int i = 1; i < 4; ++i) p.slots[(size_t) i].dest = DestOff;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process ({ 0, 0, 0, 0 }, true, dt, out);
        CHECK (count (out, MidiOut::CC) == 1 && out[0].data1 == 1 && out[0].data2 == 0);
        out.clear(); m.process ({ 0, 0, 0, 0 }, true, dt, out);
        CHECK (out.empty());
        out.clear();
        for (int b = 0; b < 200; ++b) m.process ({ 1, 0, 0, 0 }, true, dt, out);
        CHECK (out.back().data2 == 127);
        bool rising = true; for (size_t i = 1; i < out.size(); ++i) rising &= out[i].data2 > out[i - 1].data2;
        CHECK (rising && out.size() > 5);        // a glide, not a jump
        // no face: hold
        out.clear(); m.process ({ 0, 0, 0, 0 }, false, dt, out);
        CHECK (out.empty());
    }
    {   // range, invert and the other destinations
        MouthMapper m; MapperParams p; p.trigger = false;
        p.slots[0] = { Width, DestPitchBend, 0, 1.0f, 0.5f, 1.0f, false };
        p.slots[1] = { Smile, DestPressure, 0, 1.0f, 0.0f, 1.0f, true };
        p.slots[2].dest = p.slots[3].dest = DestOff;
        p.channel = 3;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process ({ 0, 0, 0, 0 }, true, dt, out);
        CHECK (out.size() == 2);
        CHECK (out[0].type == MidiOut::PitchBend && out[0].data1 == 8192 && out[0].channel == 3);
        CHECK (out[1].type == MidiOut::Pressure && out[1].data1 == 127);
        // changing the CC number resends
        p.slots[1].dest = DestCC; p.slots[1].cc = 74; m.setParams (p);
        out.clear(); m.process ({ 0, 0, 0, 0 }, true, dt, out);
        CHECK (out.size() == 1 && out[0].type == MidiOut::CC && out[0].data1 == 74 && out[0].data2 == 127);
    }
    {   // the note: on past the threshold, off below 70 % of it, off when the face goes
        MouthMapper m; MapperParams p;
        for (auto& s : p.slots) s.dest = DestOff;
        p.threshold = 0.3f; p.triggerNote = 50;
        m.setParams (p);
        std::vector<MidiOut> out;
        m.process ({ 0.1f, 0, 0, 0 }, true, dt, out);
        m.process ({ 0.29f, 0, 0, 0 }, true, dt, out);
        CHECK (out.empty());
        m.process ({ 0.35f, 0, 0, 0 }, true, dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 1 && out[0].data1 == 50);
        m.process ({ 0.25f, 0, 0, 0 }, true, dt, out);      // inside the hysteresis
        CHECK (count (out, MidiOut::NoteOff) == 0 && m.noteSounding());
        m.process ({ 0.15f, 0, 0, 0 }, true, dt, out);
        CHECK (count (out, MidiOut::NoteOff) == 1 && ! m.noteSounding());
        out.clear();
        m.process ({ 0.9f, 0, 0, 0 }, true, dt, out);
        m.process ({ 0.9f, 0, 0, 0 }, false, dt, out);
        CHECK (count (out, MidiOut::NoteOn) == 1 && count (out, MidiOut::NoteOff) == 1);
    }
    {   // a snap open plays louder than a slow one
        auto velocityAfter = [&] (int slowBlocks)
        {
            MouthMapper m; MapperParams p; for (auto& s : p.slots) s.dest = DestOff;
            p.threshold = 0.4f; m.setParams (p);
            std::vector<MidiOut> out;
            m.process ({ 0, 0, 0, 0 }, true, dt, out);
            for (int b = 0; b < slowBlocks; ++b) m.process ({ 0.3f, 0, 0, 0 }, true, dt, out);
            m.process ({ 0.5f, 0, 0, 0 }, true, dt, out);
            return out.empty() ? -1 : out.back().data2;
        };
        const int fast = velocityAfter (0), slow = velocityAfter (30);
        CHECK (fast > 110 && slow < 60 && slow > 0);
    }
    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
