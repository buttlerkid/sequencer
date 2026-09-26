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
        state[i].firstK = 0;
        state[i].lastFiredK = -1;
        state[i].pending.clear();
        state[i].rng.seed (static_cast<uint32_t> (7919 * (i + 1)));
        state[i].currentStep.store (-1);
    }
    active.clear();
    wasPlaying  = false;
    expectedPpq = 0.0;
}

double Sequencer::maxOffsetPpq (const TrackSettings& s, const GlobalSettings& g, const TrackModel& model, double msToPpq)
{
    const int steps = model.steps();
    int earliest = 0;
    for (int i = 0; i < steps; ++i)
        earliest = std::min (earliest, model.get (Lane::Timing, i));

    const double ms = -earliest + std::abs (s.shiftMs) + std::abs (g.masterShiftMs) + g.humanizeTimeMs;
    return std::min (kMaxTimingMs, ms) * msToPpq + 0.5 * s.division;
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

bool Sequencer::conditionPasses (TrackState& st, int cond, int64_t k, int steps, const GlobalSettings& g) const
{
    switch (cond)
    {
        case CondAlways:  return true;
        case CondFill:    return g.fill;
        case CondNotFill: return ! g.fill;
        case CondFirst:   return floorDiv (k, steps) == floorDiv (st.firstK, steps);
        case CondPrev:    return st.lastFiredK == k - 1;
        case CondNotPrev: return st.lastFiredK != k - 1;
        default:
        {
            int a = 0, b = 0;
            conditionRatio (cond, a, b);
            if (b <= 0) return true;
            return posMod (floorDiv (k, steps), b) == a - 1;
        }
    }
}

int Sequencer::noteFor (const TrackSettings& s, const GlobalSettings& g, int degree)
{
    if (s.pitchMode == PitchFixed)
        return clampT (s.fixedNote, 0, 127);
    if (g.chordSize > 0)
        return noteForChordDegree (g.chordNotes, g.chordSize, degree);
    return clampT (noteForDegree (g.key, g.scale, kBaseNote, degree) + g.midiTranspose, 0, 127);
}

void Sequencer::scheduleStep (int trackIdx, int64_t k, const TrackSettings& s, const GlobalSettings& g,
                              const TrackModel& model, double msToPpq)
{
    auto& st = state[trackIdx];
    const int steps  = model.steps();
    const int idx    = posMod (k, steps);
    const int pulses = model.pulses();
    const int mode   = model.euclidMode();

    const bool manual = model.isActive (idx);
    const bool eu     = pulses > 0 && euclidHit (euclidean (steps, pulses, model.rotate()), idx);
    const bool hit    = mode == EuclidOff  ? manual
                      : mode == EuclidAdd  ? (manual || eu)
                                           : eu;
    if (! hit || s.mute)
        return;

    if (! conditionPasses (st, model.get (Lane::Condition, idx), k, steps, g))
        return;

    const int prob = clampT (model.get (Lane::Probability, idx) * clampT (s.probScale, 0, 100) / 100, 0, 100);
    if (prob < 100 && static_cast<int> (st.rng() % 100u) >= prob)
        return;

    st.lastFiredK = k;

    // Humanise: symmetric random jitter, drawn from the track's own generator so
    // offline renders stay reproducible.
    double humanMs = 0.0;
    int    humanVel = 0;
    if (g.humanizeTimeMs > 0) humanMs  = (static_cast<double> (st.rng() % 2001u) / 1000.0 - 1.0) * g.humanizeTimeMs;
    if (g.humanizeVel > 0)    humanVel = static_cast<int> (st.rng() % static_cast<uint32_t> (2 * g.humanizeVel + 1)) - g.humanizeVel;

    double swing = 0.0;
    if (s.swingMode == SwingGlobal)      swing = swingOffsetSteps (g.swingProfile, g.swingAmount, k);
    else if (s.swingMode == SwingCustom) swing = swingOffsetSteps (s.swingProfile, s.swingAmount, k);

    const double t = static_cast<double> (k) * s.division
                   + swing * s.division
                   + (model.get (Lane::Timing, idx) + s.shiftMs + g.masterShiftMs + humanMs) * msToPpq;

    const int note = noteFor (s, g, s.transpose + model.get (Lane::Interval, idx));

    const int    vel = clampT (model.get (Lane::Velocity, idx) + s.velOffset + humanVel, 1, 127);
    const int    rep = clampT (model.get (Lane::Repeats, idx) + s.repsAdd, 1, 8);
    const double sub = s.division / rep;
    const double lenPct = model.get (Lane::Length, idx) * clampT (s.lengthScale, 25, 400) / 100.0;
    const double len = std::max (0.01, lenPct / 100.0 * sub);

    for (int j = 0; j < rep; ++j)
        st.pending.push_back ({ t + j * sub, clampT (s.channel, 1, 16), note, vel, len, trackIdx });
}

void Sequencer::process (const Transport& t, const GlobalSettings& g, const TrackSettingsArray& tracks,
                         const PatternSchedule& sched, std::vector<MidiEvent>& out)
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

    const PatternModel& pattern = sched.at (t.ppqStart);   // for lookahead / display

    if (jump)
    {
        allNotesOff (out, t.ppqStart);
        for (int i = 0; i < kNumTracks; ++i)
        {
            state[i].pending.clear();
            const double d = tracks[i].division;
            state[i].nextK = static_cast<int64_t> (std::floor ((t.ppqStart - maxOffsetPpq (tracks[i], g, pattern.tracks[i], msToPpq)) / d));
            state[i].firstK = static_cast<int64_t> (std::ceil (t.ppqStart / d - 1e-9));
            state[i].lastFiredK = -1;
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
        const int    steps  = pattern.tracks[i].steps();
        const double maxOff = maxOffsetPpq (s, g, pattern.tracks[i], msToPpq);

        // Division changed under us (or track was just enabled): put the cursor back in range.
        const double cursorPpq = static_cast<double> (st.nextK) * d;
        if (cursorPpq < t.ppqStart - maxOff - d || cursorPpq > ppqEnd + maxOff + d)
        {
            st.nextK = static_cast<int64_t> (std::floor ((t.ppqStart - maxOff) / d));
            st.firstK = static_cast<int64_t> (std::ceil (t.ppqStart / d - 1e-9));
            st.pending.clear();
        }

        int guard = 0;
        while (static_cast<double> (st.nextK) * d < ppqEnd + maxOff && guard++ < 4096)
        {
            scheduleStep (i, st.nextK, s, g, sched.at (static_cast<double> (st.nextK) * d + kBoundaryEps).tracks[i], msToPpq);
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
                                                 const PatternSchedule& sched, int bars)
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
        seq.process (t, g, tracks, sched, block);
        for (const auto& e : block)
            if (e.ppq >= 0.0)
                all.push_back (e);
    }
    t.ppqStart = lengthPpq;

    // Flush anything still held so every note-on has its note-off.
    t.playing = false;
    seq.process (t, g, tracks, sched, block);
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
