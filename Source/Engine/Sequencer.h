#pragma once
#include "Types.h"
#include "Pattern.h"
#include <atomic>
#include <random>
#include <vector>

namespace dy {

using TrackSettingsArray = std::array<TrackSettings, kNumTracks>;

// Converts host transport position into MIDI events. Works entirely in PPQ so the
// same code path serves real-time playback and offline MIDI export.
//
// Each track keeps a cursor `nextK` over the absolute step timeline (k * division).
// Steps are scheduled ahead by up to `maxOffset` (swing + micro-timing) so negative
// offsets never arrive late, then flushed once their real time falls inside the block.
class Sequencer
{
public:
    Sequencer() { reset(); }

    void reset();

    // Appends the events for this block to `out` (cleared first). Events carry an
    // absolute ppq; the caller converts to sample offsets.
    void process (const Transport& t,
                  const GlobalSettings& g,
                  const TrackSettingsArray& tracks,
                  const PatternModel& pattern,
                  std::vector<MidiEvent>& out);

    // Step currently under the playhead for UI display, -1 when idle.
    int currentStep (int track) const { return state[track].currentStep.load (std::memory_order_relaxed); }

    // Renders `bars` bars (4/4) from ppq 0 at the given settings using a fresh
    // engine instance. Probability uses a fixed seed so exports are repeatable.
    static std::vector<MidiEvent> renderOffline (const GlobalSettings& g,
                                                 const TrackSettingsArray& tracks,
                                                 const PatternModel& pattern,
                                                 int bars);

    static constexpr double kMaxTimingMs   = 64.0;
    static constexpr double kJumpTolerance = 0.02;   // ppq; ~10ms at 120bpm
    static constexpr double kBoundaryEps   = 1e-9;   // ppq; absorbs block-edge rounding

private:
    struct Pending { double ppq; int channel, note, velocity; double lengthPpq; int track; };
    struct Active  { double offPpq; int channel, note, track; };

    struct TrackState
    {
        int64_t nextK = 0;
        std::vector<Pending> pending;
        std::mt19937 rng;
        std::atomic<int> currentStep { -1 };
    };

    void scheduleStep (int trackIdx, int64_t k, const TrackSettings& s, const GlobalSettings& g,
                       const TrackModel& model, double msToPpq);
    void noteOn (std::vector<MidiEvent>& out, double at, const Pending& p);
    void allNotesOff (std::vector<MidiEvent>& out, double at);
    static double maxOffsetPpq (const TrackSettings& s, double msToPpq);

    std::array<TrackState, kNumTracks> state;
    std::vector<Active> active;
    bool   wasPlaying  = false;
    double expectedPpq = 0.0;
};

} // namespace dy
