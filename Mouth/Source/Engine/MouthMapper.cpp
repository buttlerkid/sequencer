#include "MouthMapper.h"
#include <algorithm>
#include <cmath>

namespace dy::mouth {

MouthShape measure (const Point* p)
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
    return s;
}

std::array<float, kNumSources> normalise (const MouthShape& s, const Calibration& c)
{
    auto unit = [] (float v) { return std::clamp (v, 0.0f, 1.0f); };
    std::array<float, kNumSources> v {};
    v[Open]   = unit ((s.gap - c.gapRest) / c.gapSpan);
    v[Width]  = unit ((s.width - c.widthRest) / c.widthSpan);
    v[Smile]  = unit ((s.lift - c.liftRest) / c.liftSpan);
    v[Pucker] = unit ((c.widthRest - s.width) / c.puckerSpan);
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

void MouthMapper::allOff (std::vector<MidiOut>& out)
{
    if (sounding >= 0) out.push_back ({ MidiOut::NoteOff, soundingChannel, sounding, 0 });
    sounding = -1;
    gateLevel = 0.0f;
}

void MouthMapper::process (const std::array<float, kNumSources>& target, bool face, double dt, std::vector<MidiOut>& out)
{
    for (size_t i = 0; i < 4; ++i)
    {
        const auto& s = params.slots[i];
        const float t = target[(size_t) std::clamp (s.source, 0, kNumSources - 1)];
        if (! primed[i]) { value[i] = t; primed[i] = true; }
        else if (face)
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

    // the note: on when the mouth opens past the threshold, off when it closes again
    const float open = face ? target[Open] : 0.0f;
    const float on = std::max (0.02f, params.threshold), off = on * 0.7f;
    if (open < on * 0.5f) { armTime = 0.0; }
    else if (sounding < 0) armTime += dt;

    if (! params.trigger || ! face)
    {
        if (sounding >= 0) allOff (out);
    }
    else if (sounding < 0 && open >= on)
    {
        int vel = params.velocity;
        if (params.velocityFromSpeed)
        {
            // half the threshold to the threshold: 30 ms is a snap, 300 ms a slow open
            const double t = std::clamp (armTime, 0.03, 0.3);
            vel = (int) std::lround (127.0 - (t - 0.03) / 0.27 * 82.0);
        }
        sounding = std::clamp (params.triggerNote, 0, 127);
        soundingChannel = params.channel;
        out.push_back ({ MidiOut::NoteOn, soundingChannel, sounding, std::clamp (vel, 1, 127) });
    }
    else if (sounding >= 0 && (open < off || params.triggerNote != sounding || params.channel != soundingChannel))
    {
        allOff (out);
    }
    gateLevel = sounding >= 0 ? open : 0.0f;
    lastOpen = open;
}

} // namespace dy::mouth
