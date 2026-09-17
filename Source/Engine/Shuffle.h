#pragma once
#include "Types.h"

namespace dy {

// A shuffle profile is a 16-position timing curve (one cycle = 16 steps of the
// track division). Values are in [-1, 1]; at 100% amount, +1 delays a step by
// half a step (MPC-style "75%" swing), -1 pushes it half a step early.
struct ShuffleProfile
{
    const char* name;
    float curve[16];
};

inline constexpr ShuffleProfile kShuffleProfiles[] = {
    // Even swing on every off-step: the classic sampler / drum-machine feel.
    { "Classic",   { 0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1 } },
    // Off-steps delayed hard, on-steps nudged slightly late too: heavy shuffle.
    { "Shuffle",   { 0, 1, .15f, 1,  0, 1, .15f, 1,  0, 1, .15f, 1,  0, 1, .15f, 1 } },
    // Everything except the downbeat drags: laid-back boom-bap.
    { "Lazy",      { 0, .8f, .2f, .8f,  .2f, .8f, .2f, .8f,  .2f, .8f, .2f, .8f,  .2f, .8f, .2f, .8f } },
    // Off-steps early: urgent, forward-leaning.
    { "Push",      { 0, -.5f, 0, -.5f,  0, -.5f, 0, -.5f,  0, -.5f, 0, -.5f,  0, -.5f, 0, -.5f } },
    // Second 16th lightly, fourth 16th fully: back-heavy lean.
    { "Lean",      { 0, .5f, 0, 1,  0, .5f, 0, 1,  0, .5f, 0, 1,  0, .5f, 0, 1 } },
    // Irregular wobble across the bar: humanised, off-kilter.
    { "Drunk",     { 0, .7f, -.3f, 1,  0, .4f, -.2f, .9f,  .1f, .8f, -.4f, .6f,  0, .5f, -.1f, 1 } },
    // Ramps inside each beat: steps bunch toward the next downbeat.
    { "Roll",      { 0, .33f, .66f, 1,  0, .33f, .66f, 1,  0, .33f, .66f, 1,  0, .33f, .66f, 1 } },
    // Swing the off-beat 8ths instead of the 16ths: half-time feel.
    { "Half-Time", { 0, 0, 1, 0,  0, 0, 1, 0,  0, 0, 1, 0,  0, 0, 1, 0 } },
};
inline constexpr int kNumShuffleProfiles = static_cast<int> (sizeof (kShuffleProfiles) / sizeof (kShuffleProfiles[0]));

// Timing offset for absolute step k, in units of one step (division).
inline double swingOffsetSteps (int profile, int amountPct, int64_t k)
{
    profile = clampT (profile, 0, kNumShuffleProfiles - 1);
    const double amt = clampT (amountPct, 0, 100) / 100.0;
    return kShuffleProfiles[profile].curve[posMod (k, 16)] * amt * 0.5;
}

} // namespace dy
