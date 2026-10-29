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

namespace
{
    Point sub (Point a, Point b) { return { a.x - b.x, a.y - b.y }; }
    float dot (Point a, Point b) { return a.x * b.x + a.y * b.y; }
    float dist (Point a, Point b) { return std::sqrt (dot (sub (a, b), sub (a, b))); }
    float unit (float v) { return std::clamp (v, 0.0f, 1.0f); }
    constexpr double kTwoPi = 6.283185307179586;
}

// ------------------------------------------------------------------ faces
MouthShape measure (const Point* p, float frameW, float frameH)
{
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
        s.headX = unit (1.0f - p[Mesh::nose].x / frameW);
        s.headY = unit (1.0f - p[Mesh::nose].y / frameH);
    }
    return s;
}

Shapes normalise (const MouthShape& s, const Calibration& c, AutoRange* ar, float dt)
{
    // how far each shape is from the neutral face, and the calibrated span for 100 %
    const float dev[5]  = { s.gap - c.gapRest, s.width - c.widthRest, s.lift - c.liftRest, c.widthRest - s.width, s.brow - c.browRest };
    const float span[5] = { c.gapSpan, c.widthSpan, c.liftSpan, c.puckerSpan, c.browSpan };
    Shapes v {};
    for (size_t i = 0; i < 5; ++i)
    {
        float full = span[i];
        if (ar != nullptr)
        {
            // the furthest this face went, forgotten over about a minute; never below a third
            // of the calibrated span (twitches stay small), never above one and a half
            auto& peak = ar->peak[i];
            peak = std::max (peak * std::exp (-dt / 60.0f), dev[i]);
            full = std::clamp (peak, 0.35f * span[i], 1.5f * span[i]);
        }
        v[i] = unit (dev[i] / full);
    }
    v[Tilt]   = unit (0.5f + (s.roll - c.rollRest) / (2.0f * c.rollSpan));
    v[HeadX]  = s.headX;
    v[HeadY]  = s.headY;
    return v;
}

// ------------------------------------------------------------------ hands
HandShape measureHand (const Point* p, float frameW, float frameH)
{
    HandShape h;
    for (int i : { HandMesh::wrist, HandMesh::indexKnuckle, HandMesh::middleKnuckle, HandMesh::ringKnuckle, HandMesh::pinkyKnuckle })
        h.palm = { h.palm.x + p[i].x * 0.2f, h.palm.y + p[i].y * 0.2f };
    const float size = dist (p[HandMesh::wrist], p[HandMesh::middleKnuckle]);
    if (size < 1.0e-6f) return h;
    if (frameW > 0 && frameH > 0) { h.x = unit (1.0f - h.palm.x / frameW); h.y = unit (1.0f - h.palm.y / frameH); }
    // thumb and index tips: touching ~0.15 palm, apart ~1
    h.pinch = unit ((0.9f - dist (p[HandMesh::thumbTip], p[HandMesh::indexTip]) / size) / 0.7f);
    // fingertips from the wrist: a fist ~1 palm, an open hand ~2
    float reach = 0;
    for (int i : { HandMesh::indexTip, HandMesh::middleTip, HandMesh::ringTip, HandMesh::pinkyTip })
        reach += dist (p[i], p[HandMesh::wrist]) / size * 0.25f;
    h.spread = unit ((reach - 1.1f) / 0.8f);
    return h;
}

void CircleDetector::reset()
{
    path.clear();
    turn = 0; lastT = -1;
    omega = 0; level = 0; spinLevel = 0.5f;
    haveAngle = false;
}

bool CircleDetector::add (Point p, double t, float frameH, Event& ev)
{
    const double dt = lastT < 0 ? 1.0 / 30.0 : std::clamp (t - lastT, 1.0e-3, 0.5);
    lastT = t;
    path.push_back ({ p, t });
    while (! path.empty() && t - path.front().second > 1.5) path.pop_front();
    auto relax = [&]
    {
        level *= (float) std::exp (-dt / 0.25);
        spinLevel = 0.5f + (spinLevel - 0.5f) * (float) std::exp (-dt / 0.25);
        omega *= (float) std::exp (-dt / 0.25);
    };
    if (path.size() < 8) { relax(); return false; }

    // the middle of the recent path, and how round it is
    Point c;
    for (auto& q : path) { c.x += q.first.x; c.y += q.first.y; }
    c = { c.x / (float) path.size(), c.y / (float) path.size() };
    float mean = 0, var = 0;
    for (auto& q : path) mean += dist (q.first, c);
    mean /= (float) path.size();
    for (auto& q : path) { const float d = dist (q.first, c) - mean; var += d * d; }
    const float spread = std::sqrt (var / (float) path.size());
    const bool round = frameH > 0 && mean > 0.025f * frameH && spread < 0.4f * mean;

    const float angle = std::atan2 (p.y - c.y, p.x - c.x);
    float d = haveAngle ? angle - lastAngle : 0.0f;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    lastAngle = angle;
    haveAngle = true;
    if (! round) { turn *= 0.5; relax(); return false; }

    if (turn != 0.0 && (d > 0) != (turn > 0) && std::abs (turn) > 0.5) turn = 0.0;   // turned back: start again
    turn += d;
    omega += (float) ((d / dt - omega) * (1.0 - std::exp (-dt / 0.15)));
    const float size = unit ((mean / frameH - 0.03f) / 0.27f);
    level += (size - level) * (float) (1.0 - std::exp (-dt / 0.1));
    // the image is mirrored for the person: their clockwise is the image's anticlockwise
    spinLevel = unit (0.5f - omega / (float) (4.0 * kTwoPi));

    if (std::abs (turn) < kTwoPi) return false;
    const int dir = turn > 0 ? -1 : 1;
    turn -= turn > 0 ? kTwoPi : -kTwoPi;
    ev = { size, unit (std::abs (omega) / (float) (2.0 * kTwoPi)), dir };   // two turns a second = full speed
    return true;
}

bool StrumDetector::add (Point p, double t, float frameH, Event& ev)
{
    path.push_back ({ p, t });
    while (! path.empty() && t - path.front().second > 0.25) path.pop_front();
    if (path.size() < 3 || frameH <= 0) return false;

    // how far the palm went up or down in the last quarter second, and how quickly
    const auto& [p0, t0] = path.front();
    const float dy = p.y - p0.y, dx = p.x - p0.x;
    const double span = std::max (1.0e-3, t - t0);
    const float travel = std::abs (dy) / frameH;
    const float speed = (float) (travel / span);                 // frame heights a second
    if (travel < 0.12f || speed < 1.2f || std::abs (dy) < 1.5f * std::abs (dx)) return false;

    const int dir = dy > 0 ? 1 : -1;
    // back and forth strums come quickly; the same way twice needs a breath between
    const double since = sinceLast (t);
    if (since < 0.12 || (dir == lastDirection && since < 0.35)) return false;
    lastStrum = t;
    lastDirection = dir;
    path.clear();
    path.push_back ({ p, t });
    ev = { unit ((speed - 1.2f) / 3.0f), dir };
    return true;
}

// ------------------------------------------------------------------ MIDI
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

int MouthMapper::channelOf (int who) const
{
    return params.faceChannels ? std::clamp (params.channel + who, 1, 16) : params.channel;
}

bool MouthMapper::press (int ch, int note, int velocity, int who, std::vector<MidiOut>& out)
{
    if (held[(size_t) ((ch - 1) * 128 + note)]++ != 0) return false;
    out.push_back ({ MidiOut::NoteOn, ch, note, std::clamp (velocity, 1, 127), who });
    return true;
}

void MouthMapper::release (int ch, int note, std::vector<MidiOut>& out)
{
    auto& h = held[(size_t) ((ch - 1) * 128 + note)];
    if (h > 0 && --h == 0) out.push_back ({ MidiOut::NoteOff, ch, note, 0 });
}

void MouthMapper::retrigger (int ch, int note, int velocity, int who, std::vector<MidiOut>& out)
{
    // a short note played again while it still rings: end it and strike it anew
    if (! press (ch, note, velocity, who, out))
    {
        out.push_back ({ MidiOut::NoteOff, ch, note, 0 });
        out.push_back ({ MidiOut::NoteOn, ch, note, std::clamp (velocity, 1, 127), who });
    }
}

void MouthMapper::noteOn (int who, int note, int velocity, std::vector<MidiOut>& out)
{
    auto& v = voices[(size_t) who];
    v.note = std::clamp (note, 0, 127);
    v.channel = channelOf (who);
    press (v.channel, v.note, velocity, who, out);
}

void MouthMapper::noteOff (int who, std::vector<MidiOut>& out)
{
    auto& v = voices[(size_t) who];
    if (v.note < 0) return;
    release (v.channel, v.note, out);
    v.note = -1;
    v.gate = 0.0f;
}

void MouthMapper::allOff (std::vector<MidiOut>& out)
{
    for (int w = 0; w < kNumWho; ++w) noteOff (w, out);
    for (auto& q : queue) if (! q.on) release (q.channel, q.note, out);
    queue.clear();
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

int MouthMapper::degreeNote (int degree) const
{
    const auto& steps = kScales[(size_t) std::clamp (params.scale, 0, (int) kScales.size() - 1)].steps;
    const int n = (int) steps.size();
    degree = std::max (0, degree);
    return std::clamp (params.handNote + 12 * (degree / n) + steps[(size_t) (degree % n)], 0, 127);
}

int MouthMapper::handDegree (float height, int& d) const
{
    const int n = (int) kScales[(size_t) std::clamp (params.scale, 0, (int) kScales.size() - 1)].steps.size();
    const int count = 2 * n + 1;
    const float y = unit (height) * (float) count;
    if (d < 0 || y < (float) d - 0.25f || y > (float) d + 1.25f)
        d = std::clamp ((int) std::floor (y), 0, count - 1);
    return d;
}

int MouthMapper::circlePitch (float size) const
{
    // the biggest circle plays the note, smaller ones climb two octaves of the scale
    const int n = (int) kScales[(size_t) std::clamp (params.scale, 0, (int) kScales.size() - 1)].steps.size();
    const int count = 2 * n + 1;
    return degreeNote (std::clamp ((int) std::lround ((1.0f - unit (size)) * (float) (count - 1)), 0, count - 1));
}

std::vector<int> MouthMapper::strumChord (const Scene& scene, int hand) const
{
    // the fretting hand: the nearest other hand across the picture (the same person's other
    // hand, mostly); its height picks the chord, one of the scale's first seven degrees
    const auto me = (size_t) handWho (hand);
    int fret = -1;
    float best = 1.0e9f;
    for (int h = 0; h < kMaxHands; ++h)
    {
        const auto w = (size_t) handWho (h);
        if (h == hand || ! scene.present[w]) continue;
        const float d = std::abs (scene.shapes[w][HandX] - scene.shapes[me][HandX]);
        if (d < best) { best = d; fret = h; }
    }
    const float height = scene.shapes[(size_t) handWho (fret >= 0 ? fret : hand)][HandY];
    const int root = std::clamp ((int) std::floor (unit (height) * 7.0f), 0, 6);
    const int n = (int) kScales[(size_t) std::clamp (params.scale, 0, (int) kScales.size() - 1)].steps.size();
    return { degreeNote (root), degreeNote (root + 2), degreeNote (root + 4), degreeNote (root + n) };
}

void MouthMapper::process (const Scene& scene, double dt, std::vector<MidiOut>& out)
{
    clock += dt;
    for (size_t i = 0; i < 4; ++i)
    {
        const auto& s = params.slots[i];
        const auto w = (size_t) std::clamp (s.who, 0, kNumWho - 1);
        const float t = scene.shapes[w][(size_t) std::clamp (s.source, 0, kNumSources - 1)];
        if (! primed[i]) { value[i] = t; primed[i] = true; }
        else if (scene.present[w])
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

    // the faces' notes: each its own, on when its mouth opens past the threshold, off when it closes
    const float on = std::max (0.02f, params.threshold), off = on * 0.7f;
    for (int f = 0; f < kMaxFaces; ++f)
    {
        auto& v = voices[(size_t) f];
        const bool present = scene.present[(size_t) f];
        const auto& s = scene.shapes[(size_t) f];
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
            if (pitch != v.note || channelOf (f) != v.channel)
            {
                const Voice old = v;
                noteOn (f, pitch, 90, out);
                release (old.channel, old.note, out);
            }
        }
        v.gate = v.note >= 0 ? open : 0.0f;
    }

    // the hands. Theremin: an open hand sings, its height the note, a fist stops it
    for (int h = 0; h < kMaxHands; ++h)
    {
        const int w = handWho (h);
        auto& v = voices[(size_t) w];
        const bool present = scene.present[(size_t) w];
        const auto& s = scene.shapes[(size_t) w];
        if (! params.handNotes || params.handMode != HandTheremin || ! present)
        {
            noteOff (w, out);
            v.degree = -1;
            continue;
        }
        const float open = s[Spread];
        if (v.note < 0 && open > 0.55f)
            noteOn (w, degreeNote (handDegree (s[HandY], v.degree)), 100, out);
        else if (v.note >= 0 && open < 0.35f)
            noteOff (w, out);
        else if (v.note >= 0)
        {
            const int pitch = degreeNote (handDegree (s[HandY], v.degree));
            if (pitch != v.note || channelOf (w) != v.channel)
            {
                const Voice old = v;
                noteOn (w, pitch, 100, out);
                release (old.channel, old.note, out);
            }
        }
        v.gate = v.note >= 0 ? open : 0.0f;
    }

    // strums and circles: short notes, through a little queue (a strum rolls its chord)
    for (int h = 0; h < kMaxHands; ++h)
    {
        const int w = handWho (h);
        const int ch = channelOf (w);
        auto fresh = [] (int count, int& seen) { const bool f = seen >= 0 && count > seen; seen = count; return f; };
        const bool strum = fresh (scene.strums[(size_t) h], strumsSeen[(size_t) h]);
        const bool circle = fresh (scene.circles[(size_t) h], circlesSeen[(size_t) h]);
        if (! params.handNotes) continue;
        if (strum && params.handMode == HandGuitar)
        {
            auto notes = strumChord (scene, h);
            const float speed = unit (scene.strumSpeed[(size_t) h]);
            const bool up = scene.strumDirection[(size_t) h] < 0;
            if (up) std::reverse (notes.begin(), notes.end());     // an up strum starts on the high string
            const int vel = (int) std::lround ((55.0f + 72.0f * speed) * (up ? 0.85f : 1.0f));
            const double roll = 0.028 - 0.016 * speed;            // a fast strum rolls tighter
            for (size_t k = 0; k < notes.size(); ++k)
            {
                queue.push_back ({ clock + roll * (double) k, true, ch, notes[k], vel, w });
                queue.push_back ({ clock + roll * (double) k + params.handLength, false, ch, notes[k], 0, w });
            }
        }
        if (circle && params.handMode == HandCircles)
        {
            const int note = circlePitch (scene.circleSize[(size_t) h]);
            const int vel = (int) std::lround (50.0f + 77.0f * unit (scene.circleSpeed[(size_t) h]));
            queue.push_back ({ clock, true, ch, note, vel, w });
            queue.push_back ({ clock + params.handLength, false, ch, note, 0, w });
        }
    }
    std::stable_sort (queue.begin(), queue.end(), [] (const Timed& a, const Timed& b) { return a.at < b.at; });
    size_t due = 0;
    for (; due < queue.size() && queue[due].at <= clock; ++due)
    {
        const auto& q = queue[due];
        if (q.on) retrigger (q.channel, q.note, q.velocity, q.who, out);
        else release (q.channel, q.note, out);
    }
    queue.erase (queue.begin(), queue.begin() + (std::ptrdiff_t) due);
}

} // namespace dy::mouth
