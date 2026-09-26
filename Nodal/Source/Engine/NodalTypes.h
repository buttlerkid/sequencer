#pragma once
// Core types for the DY Nodal engine. Pure C++17, no JUCE, so the engine can be
// unit-tested without building the plugin.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dy::nodal {

constexpr double kPi       = 3.14159265358979323846;
constexpr int    kMaxModes = 32;     // modes rendered by one resonator bank
constexpr int    kModeTable = 48;    // modes stored per body (sorted by frequency)
constexpr int    kSubBlock = 32;     // control-rate period in samples

template <typename T>
inline T clampT (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline double noteToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
inline double hzToNote (double hz)   { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
inline double dbToGain (double db)   { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g)    { return g > 1e-9 ? 20.0 * std::log10 (g) : -180.0; }

struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3 (float X, float Y, float Z = 0.0f) : x (X), y (Y), z (Z) {}
    Vec3 operator+ (Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (Vec3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const { return { x * s, y * s, z * s }; }
    float dot (Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross (Vec3 o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    float length() const { return std::sqrt (x * x + y * y + z * z); }
    Vec3 normalised() const { const float l = length(); return l > 1e-12f ? *this * (1.0f / l) : Vec3 (0, 0, 1); }
};

// Cheap deterministic noise for DSP (xorshift32).
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 0x9e3779b9u) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uniform() { return static_cast<float> (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }                                    // [-1,1)
};

} // namespace dy::nodal
