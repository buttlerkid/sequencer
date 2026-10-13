#include "PendulumEngine.h"
#include <algorithm>

namespace dy::pendulum {

void PendulumEngine::reset()
{
    sounding.clear();
    lastSlot.fill (-1e18);
    hits.fill (-1e18);
}

void PendulumEngine::setParams (const PendulumParams& p)
{
    params = p;
    params.count      = std::clamp (params.count, 1, kMaxPendulums);
    params.cycleBeats = std::clamp (params.cycleBeats, 1.0, 1024.0);
    params.swings     = std::clamp (params.swings, 1, 256);
    params.trigger    = std::clamp (params.trigger, 0, kNumTriggers - 1);
    params.quantize   = std::clamp (params.quantize, 0, kNumGrids - 1);
    params.scale      = std::clamp (params.scale, 0, kNumScales - 1);
    params.step       = std::clamp (params.step, 0, 12);
    params.noteBeats  = std::clamp (params.noteBeats, 1.0 / 64.0, 16.0);
    params.channel    = std::clamp (params.channel, 1, 16);
    params.noteMode   = std::clamp (params.noteMode, 0, 1);
    params.layers     = std::clamp (params.layers, 1, std::min (4, params.count));
}

double PendulumEngine::position (int k, double ppq) const
{
    // released together from the same side at the start of every cycle
    return std::cos (2.0 * kPi * swingsOf (k) * ppq / params.cycleBeats);
}

double PendulumEngine::coherence (double ppq) const
{
    double re = 0.0, im = 0.0;
    for (int k = 0; k < params.count; ++k)
    {
        const double a = 2.0 * kPi * swingsOf (k) * ppq / params.cycleBeats;
        re += std::cos (a); im += std::sin (a);
    }
    return std::sqrt (re * re + im * im) / params.count;
}

double PendulumEngine::lastEventBefore (int k, double ppq) const
{
    const double per = params.cycleBeats / (2.0 * swingsOf (k));
    const double o = params.trigger == TrigTurns ? 0.0 : 0.5;
    const double grid = kGrids[params.quantize].beats;
    auto j = static_cast<int64_t> (std::floor (ppq / per - o)) + 1;
    for (int tries = 0; tries < 6; ++tries, --j)
    {
        const bool even = (j % 2) == 0;
        if (params.trigger == TrigRightward && even) continue;
        if (params.trigger == TrigLeftward && ! even) continue;
        double t = (static_cast<double> (j) + o) * per;
        if (grid > 0.0) t = std::round (t / grid) * grid;
        if (t <= ppq) return t;
    }
    return -1e18;
}

int PendulumEngine::layerOf (int k) const
{
    return std::clamp (k * params.layers / std::max (1, params.count), 0, params.layers - 1);
}

int PendulumEngine::channelOf (int k) const
{
    return std::clamp (params.channel + (params.layerChannels ? layerOf (k) : 0), 1, 16);
}

int PendulumEngine::noteOf (int k) const
{
    const int idx = params.descending ? params.count - 1 - k : k;
    if (params.noteMode == 1) return std::clamp (36 + idx, 0, 127);          // one Drum Rack pad each
    const auto& sc = kScales[params.scale];
    // with layers, each group of pendulums climbs the scale from its own band
    const int layer = layerOf (k);
    auto firstOf = [&] (int L) { return (L * params.count + params.layers - 1) / params.layers; };   // first pendulum of layer L
    const int inLayer = params.layers > 1 ? (params.descending ? firstOf (layer + 1) - 1 - k : k - firstOf (layer)) : idx;
    const int degree = inLayer * params.step;
    const int band = params.layers > 1 ? layer * kLayerSpan : 0;
    const int oct = degree / sc.size, d = degree % sc.size;
    return std::clamp (params.root + band + 12 * oct + sc.steps[d], 0, 127);
}

void PendulumEngine::allNotesOff (int sample, std::vector<NoteEvent>& out)
{
    for (const auto& s : sounding) out.push_back ({ sample, s.note, 0.0f, false, -1, s.channel });
    sounding.clear();
}

void PendulumEngine::emitOffsets (double p0, double p1, int n, std::vector<NoteEvent>& out)
{
    releaseUntil (p0, p1, p1, n, out);
}

// Release the sounding notes that end before `limit` (in time order with the note ons).
void PendulumEngine::releaseUntil (double p0, double p1, double limit, int n, std::vector<NoteEvent>& out)
{
    const double span = p1 - p0;
    auto toSample = [&] (double t) { return std::clamp (static_cast<int> (std::floor ((t - p0) / span * n)), 0, n - 1); };
    for (auto it = sounding.begin(); it != sounding.end();)
    {
        if (it->offPpq < limit)
        {
            out.push_back ({ toSample (std::max (it->offPpq, p0)), it->note, 0.0f, false, -1, it->channel });
            it = sounding.erase (it);
        }
        else ++it;
    }
}

void PendulumEngine::process (double p0, double bpm, int n, std::vector<NoteEvent>& out)
{
    if (n <= 0) return;
    const size_t first = out.size();
    const double p1 = p0 + n * bpm / 60.0 / sr;
    const double span = p1 - p0;
    const double G = params.cycleBeats;
    const double grid = kGrids[params.quantize].beats;
    auto toSample = [&] (double t) { return std::clamp (static_cast<int> (std::floor ((t - p0) / span * n)), 0, n - 1); };

    // Each pendulum's events sit at t_j = (j + o) * G / (2 * swings): o = 1/2 for the
    // crossings of the middle, o = 0 for the turning points. Even j moves leftward
    // (crossings) or is the released side (turns).
    const double o = params.trigger == TrigTurns ? 0.0 : 0.5;
    const double margin = grid > 0.0 ? grid : 0.0;
    struct Hit { double t; int k; };
    std::vector<Hit> found;
    for (int k = 0; k < params.count; ++k)
    {
        const double per = G / (2.0 * swingsOf (k));
        const auto jLo = static_cast<int64_t> (std::ceil ((p0 - margin) / per - o)) - 1;
        const auto jHi = static_cast<int64_t> (std::floor ((p1 + margin) / per - o)) + 1;
        for (int64_t j = jLo; j <= jHi; ++j)
        {
            const bool even = (j % 2) == 0;
            if (params.trigger == TrigRightward && even) continue;
            if (params.trigger == TrigLeftward && ! even) continue;
            double t = (static_cast<double> (j) + o) * per;
            if (grid > 0.0) t = std::round (t / grid) * grid;
            // shifted by a hair: an event on a block boundary must land in exactly one block,
            // even when the two blocks round that boundary differently (the slot check stops repeats)
            if (t < p0 - 1e-9 || t >= p1 - 1e-9) continue;
            if (std::abs (t - lastSlot[static_cast<size_t> (k)]) < 1e-9) continue;   // two swings in one grid slot
            lastSlot[static_cast<size_t> (k)] = t;
            found.push_back ({ t, k });
        }
    }
    std::sort (found.begin(), found.end(), [] (const Hit& a, const Hit& b) { return a.t < b.t || (a.t == b.t && a.k < b.k); });

    for (const auto& h : found)
    {
        releaseUntil (p0, p1, h.t + 1e-12, n, out);       // notes that ended up to here, first
        const int note = noteOf (h.k), s = toSample (h.t), ch = channelOf (h.k);
        // a note still sounding is released first
        for (auto it = sounding.begin(); it != sounding.end(); ++it)
            if (it->note == note && it->channel == ch)
            {
                out.push_back ({ s, note, 0.0f, false, h.k, ch });
                sounding.erase (it);
                break;
            }
        const float coh = static_cast<float> (coherence (h.t));
        const float vel = std::clamp (params.velocity * (1.0f - params.accent * (1.0f - coh)), 1.0f / 127.0f, 1.0f);
        out.push_back ({ s, note, vel, true, h.k, ch });
        sounding.push_back ({ note, ch, h.t + params.noteBeats });
        hits[static_cast<size_t> (h.k)] = h.t;
    }
    // notes shorter than the rest of the block end inside it
    emitOffsets (p0, p1, n, out);

    std::stable_sort (out.begin() + static_cast<std::ptrdiff_t> (first), out.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.sample != b.sample) return a.sample < b.sample;
        return ! a.on && b.on;                         // offs before ons on the same sample
    });
}

} // namespace dy::pendulum
