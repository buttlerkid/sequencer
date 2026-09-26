#pragma once
#include "Pattern.h"
#include <algorithm>
#include <random>
#include <vector>

namespace dy {

enum class ArpMode : int { Up = 0, Down, UpDown, Random };

// Pattern generation helpers. Run on the message thread; they write straight into
// the atomics of a TrackModel.
class Generator
{
public:
    explicit Generator (uint32_t seed = std::random_device{}()) : rng (seed) {}

    void randomizeSteps (TrackModel& t, int steps, int densityPct)
    {
        for (int i = 0; i < clampT (steps, 1, kMaxSteps); ++i)
            t.setActive (i, static_cast<int> (rng() % 100u) < densityPct);
    }

    void randomizeLane (TrackModel& t, Lane lane, int steps, int lo, int hi)
    {
        const auto& info = laneInfo (lane);
        lo = clampT (lo, info.min, info.max);
        hi = clampT (hi, lo, info.max);
        for (int i = 0; i < clampT (steps, 1, kMaxSteps); ++i)
            t.set (lane, i, lo + static_cast<int> (rng() % static_cast<uint32_t> (hi - lo + 1)));
    }

    // Writes chord-tone intervals (root, 3rd, 5th, then the next octave) across all
    // `steps`, so whichever steps end up firing (manual or Euclid) trace an arpeggio.
    // In chord-follow mode Interval indexes the held chord, so consecutive degrees
    // are used instead of scale thirds.
    void arpeggiate (TrackModel& t, int steps, ArpMode mode, int octaves = 2, bool chordFollow = false)
    {
        std::vector<int> seq;
        if (chordFollow)
            for (int i = 0; i < 3 * clampT (octaves, 1, 3); ++i) seq.push_back (i);
        else
            for (int o = 0; o < clampT (octaves, 1, 3); ++o)
                for (int c : { 0, 2, 4 })
                    seq.push_back (c + o * 7);

        if (mode == ArpMode::Down)
            std::reverse (seq.begin(), seq.end());
        else if (mode == ArpMode::UpDown)
        {
            const int n = static_cast<int> (seq.size());
            for (int i = n - 2; i >= 1; --i)
                seq.push_back (seq[static_cast<size_t> (i)]);
        }

        steps = clampT (steps, 1, kMaxSteps);
        for (int i = 0; i < steps; ++i)
        {
            const size_t pick = mode == ArpMode::Random ? static_cast<size_t> (rng() % seq.size())
                                                        : static_cast<size_t> (i) % seq.size();
            t.set (Lane::Interval, i, seq[pick]);
        }
    }

private:
    std::mt19937 rng;
};

} // namespace dy
