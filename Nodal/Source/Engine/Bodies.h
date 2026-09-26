#pragma once
#include "NodalTypes.h"
#include <string>
#include <vector>

namespace dy::nodal {

// The vibrating bodies. 2D plates live in the plane (x, y in [-1, 1]); 3D bodies
// are shells whose surface points are given by a direction from the centre.
enum class Body : int { Square = 0, Circle, Hexagon, Violin, Sphere, Cube, Icosahedron, Count };
constexpr int kNumBodies = static_cast<int> (Body::Count);

inline bool is3D (Body b) { return static_cast<int> (b) >= static_cast<int> (Body::Sphere); }
const char* bodyName (Body b);

// One vibration mode. The meaning of a/b/c depends on the body:
//   Square, Hexagon  (a, b) wave numbers, variant = sign of the mirrored term
//   Violin           (a, b) wave numbers across / along the body
//   Circle           a = nodal diameters, b = nodal circles, variant 0 cos / 1 sin
//   Sphere, Icosa    a = degree l, b = order m (signed)
//   Cube             (a, b, c) wave numbers along x, y, z
struct Mode
{
    int a = 0, b = 0, c = 0, variant = 0;
    double ratio = 1.0;            // frequency relative to the lowest mode
    float  norm  = 1.0f;           // 1 / max |shape|, so shapes peak at +-1
    double besselZero = 0.0;       // circle only
    std::vector<float> radial;     // circle only: J_a (besselZero * r) sampled over r in [0, 1]
};

struct ModeSet
{
    Body body = Body::Square;
    std::vector<Mode> modes;       // kModeTable entries, ascending frequency
};

// Built once per process and cached; safe to call from any thread after the
// first call (the processor warms the cache in its constructor).
const ModeSet& modeSet (Body b);

// Normalised mode shape at a point. For 2D bodies p.z is ignored and points outside
// the plate return 0. For 3D bodies p is a direction (it is normalised internally).
float modeShape (Body b, const Mode& m, Vec3 p);

bool  insidePlate (Body b, float x, float y);   // 2D only
Vec3  surfacePoint (Body b, Vec3 dir);           // 3D only: where a direction meets the shell

// Strike position from two knobs in [-1, 1]. 2D: a point inside the plate.
// 3D: azimuth / elevation turned into a direction.
Vec3  strikePoint (Body b, float sx, float sy);
// Stereo pickups placed opposite the strike, spread apart by `spread` (0..1).
void  pickupPoints (Body b, Vec3 strike, float spread, Vec3& left, Vec3& right);

// "N 2 · M 3" style label for a mode.
std::string modeLabel (Body b, const Mode& m);

// Bessel function of the first kind, integer order (exact to ~1e-10 for x < 60).
double besselJ (int n, double x);

} // namespace dy::nodal
