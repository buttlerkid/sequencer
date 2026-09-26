#pragma once

namespace dy::nodal {

// How a material behaves as a resonating body.
//   alpha   high modes die faster: T60(f) = decay * (f / f0)^-alpha
//   stretch mode ratios are raised to (1 + stretch): stiffer = sharper overtones
//   bright  tilt of the mode amplitudes, -1 dark .. +1 bright
//   decay   multiplier on the Decay control
struct Material
{
    const char* name;
    double alpha, stretch, bright, decay;
};

inline constexpr Material kMaterials[] = {
    { "Steel",   0.55,  0.000,  0.15, 1.00 },
    { "Brass",   0.70,  0.004,  0.00, 0.85 },
    { "Glass",   0.40, -0.004,  0.35, 1.15 },
    { "Crystal", 0.25, -0.002,  0.55, 1.60 },
    { "Ceramic", 1.00,  0.010,  0.10, 0.45 },
    { "Wood",    1.50,  0.020, -0.40, 0.22 },
};
inline constexpr int kNumMaterials = static_cast<int> (sizeof (kMaterials) / sizeof (kMaterials[0]));

} // namespace dy::nodal
