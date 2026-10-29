#include "MouthMapper.h"
#include <algorithm>
#include <cmath>

namespace dy::mouth {

const std::vector<NamedSet> kChords {
    { "Unison", { 0 } },        { "Major", { 0, 4, 7, 12 } }, { "Minor", { 0, 3, 7, 12 } },
    { "Sus4", { 0, 5, 7, 12 } }, { "Fifths", { 0, 7, 12, 19 } }, { "Octaves", { 0, 12, -12, 24 } },
};

const std::vector<NamedSet> kScales {
    { "Major", { 0, 2, 4, 5, 7, 9, 11 } }, { "Minor", { 0, 2, 3, 5, 7, 8, 10 } }, { "Pentatonic", { 0, 2, 4, 7, 9 } },
    { "Minor penta", { 0, 3, 5, 7, 10 } }, { "Blues", { 0, 3, 5, 6, 7, 10 } },
    { "Chromatic", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};

MouthShape measure (const Point* p, float frameW, float frameH)
{
    auto sub = [] (Point a, Point b) { return Point { a.x - b.x, a.y - b.y }; };
    auto dot = [] (Point a, Point b) { return a.x * b.x + a.y * b.y; };

    const Point eyes = sub (p[Mesh::eyeL], p[Mesh::eyeR]);
    const float iod = std::sqrt (dot (eyes, eyes));
    if (iod < 1.0e-6f) return {};
    const Point ex { eyes.x / iod, eyes.y / iod };     // across the face
    const Point ey { -ex.y, ex.x };                    // down the face

    const Point corners { 0.5f * (p[Mesh::cornerR].x + p[Mesh::cornerL].x), 0.5f * (p[Mesh::cornerR].y + p[Mesh::cornerL].y) };
    MouthShape s;
    s.gap   = std::max (0.0f, dot (sub (p[Mesh::innerBottom], p[Mesh::innerTop]), ey) / iod);
    s.width = dot (sub (p[Mesh::cornerL], p[Mesh::cornerR]), ex) / iod;
    s.lift  = dot (sub (p[Mesh::lipTop], corners), ey) / iod;
    s.brow  = 0.5f * (dot (sub (p[Mesh::lidR], p[Mesh::browR]), ey) + dot (sub (p[Mesh::lidL], p[Mesh::browL]), ey)) / iod;
    // the person's right eye is on the image's left: tilting to the right shoulder drops it
    s.roll  = -std::atan2 (ex.y, ex.x);
    if (frameW > 0 && frameH > 0)
    {
        s.headX = std::clamp (1.0f - p[Mesh::nose].x / frameW, 0.0f, 1.0f);
        s.headY = std::clamp (1.0f - p[Mesh::nose].y / frameH, 0.0f, 1.0f);
    }
    return s;
}

Shapes normalise (const MouthShape& s, const Calibration& c)
{
    auto unit = [] (float v) { return std::clamp (v, 0.0f, 1.0f); };
    Shapes v {};
    v[Open]   = unit ((s.gap - c.gapRest) / c.gapSpan);
    v[Width]  = unit ((s.width - c.widthRest) / c.widthSpan);
    v[Smile]  = unit ((s.lift - c.liftRest) / c.liftSpan);
    v[Pucker] = unit ((c.widthRest - s.width) / c.puckerSpan);
    v[Brows]  = unit ((s.brow - c.browRest) / c.browSpan);
    v[Tilt]   = unit (0.5f + (s.roll - c.rollRest) / (2.0f * c.rollSpan));
    v[HeadX]  = s.headX;
    v[HeadY]  = s.headY;
    return v;
}

void MouthMapper::setParams (const MapperParams& p)
{
    for (size_t i = 0; i < 4; ++i)
    {
        const auto& a = params.slots[i];
        const auto& b = p.slots[i];
        if (a.dest != b.dest || a.cc != b.cc || params.channel != p.channel)
            lastSent[i] = -1;                    // send the current value on the new target
    }
    params = p;
    params.channel = std::clamp (params.channel, 1, 16);
}

void MouthMapper::noteOn (int face, int note, int velocity, std::vector<MidiOut>& out)
{
    auto& v = voices[(size_t) face];
    v.note = std::clamp (note, 0, 127);
    v.channel = params.faceChannels ? std::clamp (params.channel + face, 1, 16) : params.channel;
    if (held[(size_t) ((v.channel - 1) * 128 + v.note)]++ == 0)
        out.push_back ({ MidiOut::NoteOn, v.channel, v.note, std::clamp (velocity, 1, 127) });
}

void MouthMapper::noteOff (int face, std::vector<MidiOut>& out)
{
    auto& v = voices[(size_t) face];
    if (v.note < 0) return;
    auto& h = held[(size_t) ((v.channel - 1) * 128 + v.note)];
    if (h > 0 && --h == 0) out.push_back ({ MidiOut::NoteOff, v.channel, v.note, 0 });
    v.note = -1;
    v.gate = 0.0f;
}

void MouthMapper::allOff (std::vector<MidiOut>& out)
{
    for (int f = 0; f < kMaxFaces; ++f) noteOff (f, out);
}

int MouthMapper::pitchFor (int face, const Shapes& s)
{
    if (params.pitchMode == PitchChord)
    {
        const auto& chord = kChords[(size_t) std::clamp (params.chord, 0, (int) kChords.size() - 1)].steps;
        return std::clamp (params.triggerNote + chord[(size_t) face % chord.size()], 0, 127);
    }
    // Head height: two octaves of the scale from the note, bottom of the picture to the top
    const auto& steps = kScales[(size_t) std::clamp (params.scale, 0, (int) kScales.size() - 1)].steps;
    const int n = (int) steps.size(), count = 2 * n + 1;
    const float y = s[HeadY] * (float) count;
    auto& d = voices[(size_t) face].degree;
    if (d < 0 || y < (float) d - 0.25f || y > (float) d + 1.25f)
        d = std::clamp ((int) std::floor (y), 0, count - 1);
    return std::clamp (params.triggerNote + 12 * (d / n) + steps[(size_t) (d % n)], 0, 127);
}

void MouthMapper::process (const Faces& faces, double dt, std::vector<MidiOut>& out)
{
    for (size_t i = 0; i < 4; ++i)
    {
        const auto& s = params.slots[i];
        const auto f = (size_t) std::clamp (s.face, 0, kMaxFaces - 1);
        const float t = faces.shapes[f][(size_t) std::clamp (s.source, 0, kNumSources - 1)];
        if (! primed[i]) { value[i] = t; primed[i] = true; }
        else if (faces.present[f])
        {
            const double tau = std::max (0.001, s.smoothMs / 1000.0);
            value[i] += (t - value[i]) * (float) (1.0 - std::exp (-dt / tau));
        }
        const float v = s.invert ? 1.0f - value[i] : value[i];
        output[i] = std::clamp (s.lo + (s.hi - s.lo) * v, 0.0f, 1.0f);

        int code = -1;
        switch (s.dest)
        {
            case DestCC:        code = (int) std::lround (output[i] * 127.0f); break;
            case DestPressure:  code = (int) std::lround (output[i] * 127.0f); break;
            case DestPitchBend: code = (int) std::lround (output[i] * 16383.0f); break;
            default: break;
        }
        if (code < 0 || code == lastSent[i]) continue;
        lastSent[i] = code;
        if (s.dest == DestCC)             out.push_back ({ MidiOut::CC, params.channel, std::clamp (s.cc, 0, 127), code });
        else if (s.dest == DestPressure)  out.push_back ({ MidiOut::Pressure, params.channel, code, 0 });
        else                              out.push_back ({ MidiOut::PitchBend, params.channel, code, 0 });
    }

    // the notes: each face's own, on when its mouth opens past the threshold, off when it closes
    const float on = std::max (0.02f, params.threshold), off = on * 0.7f;
    for (int f = 0; f < kMaxFaces; ++f)
    {
        auto& v = voices[(size_t) f];
        const bool present = faces.present[(size_t) f];
        const auto& s = faces.shapes[(size_t) f];
        const float open = present ? s[Open] : 0.0f;
        if (open < on * 0.5f) v.armTime = 0.0;
        else if (v.note < 0) v.armTime += dt;

        if (! params.trigger || ! present)
        {
            noteOff (f, out);
            if (! present) v.degree = -1;
        }
        else if (v.note < 0 && open >= on)
        {
            int vel = params.velocity;
            if (params.velocityFromSpeed)
            {
                // half the threshold to the threshold: 30 ms is a snap, 300 ms a slow open
                const double t = std::clamp (v.armTime, 0.03, 0.3);
                vel = (int) std::lround (127.0 - (t - 0.03) / 0.27 * 82.0);
            }
            noteOn (f, pitchFor (f, s), vel, out);
        }
        else if (v.note >= 0 && open < off)
        {
            noteOff (f, out);
        }
        else if (v.note >= 0)
        {
            // still singing: follow a new pitch (or channel) legato, the new note before the old one ends
            const int pitch = pitchFor (f, s);
            const int ch = params.faceChannels ? std::clamp (params.channel + f, 1, 16) : params.channel;
            if (pitch != v.note || ch != v.channel)
            {
                const Voice old = v;
                noteOn (f, pitch, 90, out);
                auto& h = held[(size_t) ((old.channel - 1) * 128 + old.note)];
                if (h > 0 && --h == 0) out.push_back ({ MidiOut::NoteOff, old.channel, old.note, 0 });
            }
        }
        v.gate = v.note >= 0 ? open : 0.0f;
    }
}

} // namespace dy::mouth
