#pragma once
#include "Types.h"
#include <atomic>

namespace dy {

// Plain copy of one step, used for clipboard / serialisation.
struct StepData
{
    bool active = false;
    int  lane[static_cast<int> (Lane::Count)] = { 100, 80, 0, 100, 1, 0, 0 };
};

using TrackSnapshot = std::array<StepData, kMaxSteps>;

// Per-step data for one track. Every field is an atomic so the editor can write
// while the audio thread reads without locks. Not copyable; use snapshot()/load().
class TrackModel
{
public:
    TrackModel() { clear(); }

    TrackModel (const TrackModel&) = delete;
    TrackModel& operator= (const TrackModel&) = delete;

    bool isActive (int step) const                  { return active[idx (step)].load (std::memory_order_relaxed) != 0; }
    void setActive (int step, bool on)              { active[idx (step)].store (on ? 1 : 0, std::memory_order_relaxed); }
    void toggle (int step)                          { setActive (step, ! isActive (step)); }

    int  get (Lane l, int step) const               { return lanes[static_cast<int> (l)][idx (step)].load (std::memory_order_relaxed); }
    void set (Lane l, int step, int value)
    {
        const auto& info = laneInfo (l);
        lanes[static_cast<int> (l)][idx (step)].store (static_cast<int16_t> (clampT (value, info.min, info.max)),
                                                        std::memory_order_relaxed);
    }

    void resetLane (Lane l)
    {
        for (int i = 0; i < kMaxSteps; ++i) set (l, i, laneInfo (l).def);
    }

    void clearSteps()
    {
        for (int i = 0; i < kMaxSteps; ++i) setActive (i, false);
    }

    void clear()
    {
        clearSteps();
        for (int l = 0; l < static_cast<int> (Lane::Count); ++l) resetLane (static_cast<Lane> (l));
    }

    TrackSnapshot snapshot() const
    {
        TrackSnapshot s;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            s[i].active = isActive (i);
            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
                s[i].lane[l] = get (static_cast<Lane> (l), i);
        }
        return s;
    }

    void load (const TrackSnapshot& s)
    {
        for (int i = 0; i < kMaxSteps; ++i)
        {
            setActive (i, s[i].active);
            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
                set (static_cast<Lane> (l), i, s[i].lane[l]);
        }
    }

private:
    static int idx (int step) { return clampT (step, 0, kMaxSteps - 1); }

    std::atomic<uint8_t> active[kMaxSteps];
    std::atomic<int16_t> lanes[static_cast<int> (Lane::Count)][kMaxSteps];
};

struct PatternModel
{
    TrackModel tracks[kNumTracks];
};

} // namespace dy
