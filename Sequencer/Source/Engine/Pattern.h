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

struct TrackSnapshot
{
    std::array<StepData, kMaxSteps> steps;
    int length = 16, pulses = 0, rotate = 0, euclidMode = EuclidAdd;
};

// Per-step data plus length / Euclid settings for one track within one pattern.
// Every field is an atomic so the editor can write while the audio thread reads
// without locks. Not copyable; use snapshot()/load().
class TrackModel
{
public:
    TrackModel() { clear(); resetSettings(); }

    int  steps() const              { return length.load (std::memory_order_relaxed); }
    int  pulses() const             { return pulseCount.load (std::memory_order_relaxed); }
    int  rotate() const             { return rotation.load (std::memory_order_relaxed); }
    int  euclidMode() const         { return euclid.load (std::memory_order_relaxed); }
    void setSteps (int n)           { length.store (static_cast<int16_t> (clampT (n, 1, kMaxSteps)), std::memory_order_relaxed); }
    void setPulses (int n)          { pulseCount.store (static_cast<int16_t> (clampT (n, 0, kMaxSteps)), std::memory_order_relaxed); }
    void setRotate (int n)          { rotation.store (static_cast<int16_t> (clampT (n, 0, kMaxSteps - 1)), std::memory_order_relaxed); }
    void setEuclidMode (int m)      { euclid.store (static_cast<int16_t> (clampT (m, 0, 2)), std::memory_order_relaxed); }
    void resetSettings()            { setSteps (16); setPulses (0); setRotate (0); setEuclidMode (EuclidAdd); }

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
            s.steps[i].active = isActive (i);
            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
                s.steps[i].lane[l] = get (static_cast<Lane> (l), i);
        }
        s.length = steps(); s.pulses = pulses(); s.rotate = rotate(); s.euclidMode = euclidMode();
        return s;
    }

    void load (const TrackSnapshot& s)
    {
        for (int i = 0; i < kMaxSteps; ++i)
        {
            setActive (i, s.steps[i].active);
            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
                set (static_cast<Lane> (l), i, s.steps[i].lane[l]);
        }
        setSteps (s.length); setPulses (s.pulses); setRotate (s.rotate); setEuclidMode (s.euclidMode);
    }

private:
    static int idx (int step) { return clampT (step, 0, kMaxSteps - 1); }

    std::atomic<uint8_t> active[kMaxSteps];
    std::atomic<int16_t> lanes[static_cast<int> (Lane::Count)][kMaxSteps];
    std::atomic<int16_t> length { 16 }, pulseCount { 0 }, rotation { 0 }, euclid { EuclidAdd };
};

struct PatternModel
{
    TrackModel tracks[kNumTracks];
};

} // namespace dy
