#pragma once
#include "Types.h"

namespace dy {

struct ScaleDef
{
    const char* name;
    int size;
    int intervals[12];
};

inline constexpr ScaleDef kScales[] = {
    { "Major",            7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Minor",            7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorian",           7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian",         7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Lydian",           7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "Mixolydian",       7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "Locrian",          7, { 0, 1, 3, 5, 6, 8, 10 } },
    { "Harmonic Minor",   7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Melodic Minor",    7, { 0, 2, 3, 5, 7, 9, 11 } },
    { "Major Pentatonic", 5, { 0, 2, 4, 7, 9 } },
    { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 } },
    { "Blues",            6, { 0, 3, 5, 6, 7, 10 } },
    { "Whole Tone",       6, { 0, 2, 4, 6, 8, 10 } },
    { "Chromatic",       12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};
inline constexpr int kNumScales = static_cast<int> (sizeof (kScales) / sizeof (kScales[0]));

inline constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// MIDI note for an arbitrary (possibly negative) scale degree. Degrees beyond the
// scale size wrap into neighbouring octaves.
inline int noteForDegree (int key, int scaleIdx, int baseNote, int degree)
{
    const auto& s = kScales[clampT (scaleIdx, 0, kNumScales - 1)];
    const int64_t oct = floorDiv (degree, s.size);
    const int     deg = posMod (degree, s.size);
    const int note = baseNote + clampT (key, 0, 11) + s.intervals[deg] + static_cast<int> (oct) * 12;
    return clampT (note, 0, 127);
}

// Nearest in-scale note to `note` (ties resolve downward).
inline int quantizeToScale (int note, int key, int scaleIdx)
{
    const auto& s = kScales[clampT (scaleIdx, 0, kNumScales - 1)];
    int best = note, bestDist = 128;
    for (int oct = -1; oct <= 1; ++oct)
        for (int i = 0; i < s.size; ++i)
        {
            const int cand = (note / 12 + oct) * 12 + key + s.intervals[i];
            const int dist = std::abs (cand - note);
            if (dist < bestDist || (dist == bestDist && cand < best))
            {
                best = cand;
                bestDist = dist;
            }
        }
    return clampT (best, 0, 127);
}

} // namespace dy
