#include "Sequencer.h"
#include "Euclid.h"
#include "Scale.h"
#include "Shuffle.h"
#include <algorithm>

namespace dy {

void Sequencer::reset()
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        state[i].nextK = 0;
        state[i].pending.clear();
        state[i].rng.seed (static_cast<uint32_t> (7919 * (i + 1)));
        state[i].currentStep.store (-1);
    }
    active.clear();
    wasPlaying  = false;
    expectedPpq = 0.0;
}

double Sequencer::maxOffsetPpq (const TrackSettings& s, double msToPpq)
{
    return kMaxTimingMs * msToPpq + 0.5 * s.division;
}

void Sequencer::allNotesOff (std::vector<MidiEvent>& out, double at)
{
    for (const auto& a : active)
        out.push_back ({ at, false, a.channel, a.note, 0, a.track });
    active.clear();
}

void Sequencer::noteOn (std::vector<MidiEvent>& out, double at, const Pending& p)
{
    // Re-triggering a held note: release it first so nothing gets stuck.
    for (auto it = active.begin(); it != active.end();)
    {
        if (it->channel == p.channel && it->note == p.note)
        {
            out.push_back ({ at, false, p.channel, p.note, 0, it->track });
            it = active.erase (it);
        }
        else
            ++it;
    }
    out.push_back ({ at, true, p.channel, p.note, p.velocity, p.track });
    active.push_back ({ at + p.lengthPpq, p.channel, p.note, p.track });
}

void Sequencer::scheduleStep (int trackIdx, int64_t k, const TrackSettings& s, const GlobalSettings& g,
                              const TrackModel& model, double msToPpq)
{
    auto& st = state[trackIdx];
    const int steps = clampT (s.steps, 1, kMaxSteps);
    const int idx   = posMod (k, steps);

    const bool manual = model.isActive (idx);
    const bool eu     = s.pulses > 0 && euclidHit (euclidean (steps, s.pulses, s.rotate), idx);
    const bool hit    = s.euclidMode == EuclidOff  ? manual
                      : s.euclidMode == EuclidAdd  ? (manual || eu)
                                                   : eu;
    if (! hit || s.mute)
        return;

    const int prob = clampT (model.get (Lane::Probability, idx) * clampT (s.probScale, 0, 100) / 100, 0, 100);
    if (prob < 100 && static_cast<int> (st.rng() % 100u) >= prob)
        return;

    double swing = 0.0;
    if (s.swingMode == SwingGlobal)      swing = swingOffsetSteps (g.swingProfile, g.swingAmount, k);
    else if (s.swingMode == SwingCustom) swing = swingOffsetSteps (s.swingProfile, s.swingAmount, k);

    const double t = static_cast<double> (k) * s.division
                   + swing * s.division
                   + (model.get (Lane::Timing, idx) + s.shiftMs + g.masterShiftMs) * msToPpq;

    const int note = s.pitchMode == PitchFixed
                       ? clampT (s.fixedNote, 0, 127)
                       : noteForDegree (g.key, g.scale, kBaseNote, s.transpose + model.get (Lane::Interval, idx));

    const int    vel = clampT (model.get (Lane::Velocity, idx) + s.velOffset, 1, 127);
    const int    rep = clampT (model.get (Lane::Repeats, idx) + s.repsAdd, 1, 8);
    const double sub = s.division / rep;
    const double lenPct = model.get (Lane::Length, idx) * clampT (s.lengthScale, 25, 400) / 100.0;
    const double len = std::max (0.01, lenPct / 100.0 * sub);

    for (int j = 0; j < rep; ++j)
        st.pending.push_back ({ t + j * sub, clampT (s.channel, 1, 16), note, vel, len, trackIdx });
}

void Sequencer::process (const Transport& t, const GlobalSettings& g, const TrackSettingsArray& tracks,
                         const PatternModel& pattern, std::vector<MidiEvent>& out)
{
    out.clear();

    if (! t.playing || t.numSamples <= 0)
    {
        if (wasPlaying)
            allNotesOff (out, t.ppqStart);
        wasPlaying = false;
        for (auto& st : state)
        {
            st.pending.clear();
            st.currentStep.store (-1, std::memory_order_relaxed);
        }
        return;
    }

    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
    const double ppqEnd       = t.ppqStart + t.numSamples * ppqPerSample;
    const double msToPpq      = g.bpm / 60000.0;
    // Block interval is [ppqStart, ppqEnd). An event sitting exactly on ppqEnd (up to
    // rounding) belongs to the next block, where it lands on sample 0.
    const double emitBefore   = ppqEnd - kBoundaryEps;
    const bool   jump         = ! wasPlaying || std::abs (t.ppqStart - expectedPpq) > kJumpTolerance;

    if (jump)
    {
        allNotesOff (out, t.ppqStart);
        for (int i = 0; i < kNumTracks; ++i)
        {
            state[i].pending.clear();
            const double d = tracks[i].division;
            state[i].nextK = static_cast<int64_t> (std::floor ((t.ppqStart - maxOffsetPpq (tracks[i], msToPpq)) / d));
        }
    }

    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto& s  = tracks[i];
        auto&       st = state[i];

        if (! s.enabled)
        {
            st.pending.clear();
            st.currentStep.store (-1, std::memory_order_relaxed);
            continue;
        }

        const double d      = s.division;
        const int    steps  = clampT (s.steps, 1, kMaxSteps);
        const double maxOff = maxOffsetPpq (s, msToPpq);

        // Division changed under us (or track was just enabled): put the cursor back in range.
        const double cursorPpq = static_cast<double> (st.nextK) * d;
        if (cursorPpq < t.ppqStart - maxOff - d || cursorPpq > ppqEnd + maxOff + d)
        {
            st.nextK = static_cast<int64_t> (std::floor ((t.ppqStart - maxOff) / d));
            st.pending.clear();
        }

        int guard = 0;
        while (static_cast<double> (st.nextK) * d < ppqEnd + maxOff && guard++ < 4096)
        {
            scheduleStep (i, st.nextK, s, g, pattern.tracks[i], msToPpq);
            ++st.nextK;
        }

        std::sort (st.pending.begin(), st.pending.end(),
                   [] (const Pending& a, const Pending& b) { return a.ppq < b.ppq; });

        auto it = st.pending.begin();
        for (; it != st.pending.end() && it->ppq < emitBefore; ++it)
        {
            // Steps already in the past (transport start / jump) are dropped, not fired late.
            if (it->ppq >= t.ppqStart - kBoundaryEps)
                noteOn (out, std::max (it->ppq, t.ppqStart), *it);
        }
        st.pending.erase (st.pending.begin(), it);

        st.currentStep.store (posMod (static_cast<int64_t> (std::floor (t.ppqStart / d)), steps),
                              std::memory_order_relaxed);
    }

    for (auto it = active.begin(); it != active.end();)
    {
        if (it->offPpq < emitBefore)
        {
            out.push_back ({ std::max (it->offPpq, t.ppqStart), false, it->channel, it->note, 0, it->track });
            it = active.erase (it);
        }
        else
            ++it;
    }

    // Note-offs before note-ons at identical times.
    std::stable_sort (out.begin(), out.end(), [] (const MidiEvent& a, const MidiEvent& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        return ! a.noteOn && b.noteOn;
    });

    expectedPpq = ppqEnd;
    wasPlaying  = true;
}

std::vector<MidiEvent> Sequencer::renderOffline (const GlobalSettings& g, const TrackSettingsArray& tracks,
                                                 const PatternModel& pattern, int bars)
{
    Sequencer seq;
    std::vector<MidiEvent> all, block;

    const double lengthPpq    = 4.0 * clampT (bars, 1, 64);
    const int    blockSamples = 256;
    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;

    const int64_t totalSamples = static_cast<int64_t> (std::llround (lengthPpq / ppqPerSample));

    Transport t;
    t.playing = true;

    // Walk in sample counts so the final block ends exactly on the last bar line.
    for (int64_t pos = 0; pos < totalSamples; pos += blockSamples)
    {
        t.ppqStart   = static_cast<double> (pos) * ppqPerSample;
        t.numSamples = static_cast<int> (std::min<int64_t> (blockSamples, totalSamples - pos));
        seq.process (t, g, tracks, pattern, block);
        for (const auto& e : block)
            if (e.ppq >= 0.0)
                all.push_back (e);
    }
    t.ppqStart = lengthPpq;

    // Flush anything still held so every note-on has its note-off.
    t.playing = false;
    seq.process (t, g, tracks, pattern, block);
    for (auto& e : block)
    {
        e.ppq = lengthPpq;
        all.push_back (e);
    }

    std::stable_sort (all.begin(), all.end(), [] (const MidiEvent& a, const MidiEvent& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        return ! a.noteOn && b.noteOn;
    });
    return all;
}

} // namespace dy
