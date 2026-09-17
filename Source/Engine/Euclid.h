#pragma once
#include "Types.h"

namespace dy {

// Euclidean rhythm E(pulses, steps) as a 64-bit mask (bit i set = hit on step i),
// rotated right by `rotation` steps. Uses the "bucket" formulation, which yields
// the same distributions as Bjorklund's algorithm up to rotation.
inline uint64_t euclidean (int steps, int pulses, int rotation)
{
    steps  = clampT (steps, 1, kMaxSteps);
    pulses = clampT (pulses, 0, steps);
    if (pulses == 0) return 0;

    uint64_t mask = 0;
    for (int i = 0; i < steps; ++i)
        if ((i * pulses) % steps < pulses)
            mask |= (1ULL << i);

    if (pulses == steps) return mask;

    rotation = posMod (rotation, steps);
    if (rotation == 0) return mask;

    uint64_t rotated = 0;
    for (int i = 0; i < steps; ++i)
        if (mask & (1ULL << i))
            rotated |= (1ULL << ((i + rotation) % steps));
    return rotated;
}

inline bool euclidHit (uint64_t mask, int step) { return (mask >> step) & 1ULL; }

} // namespace dy
