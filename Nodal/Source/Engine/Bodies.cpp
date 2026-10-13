#include "Bodies.h"
#include "PlateModes.inc"
#include <array>
#include <cstdio>

namespace dy::nodal {

const char* bodyName (Body b)
{
    static const char* names[kNumBodies] = { "Square", "Circle", "Hexagon", "Violin", "Sphere", "Cube", "Icosahedron" };
    return names[clampT (static_cast<int> (b), 0, kNumBodies - 1)];
}

// ------------------------------------------------------------------ special functions
double besselJ (int n, double x)
{
    // J_n(x) = 1/(2 pi) * integral over a full period of cos(n t - x sin t).
    // The trapezoid rule on a periodic analytic integrand converges exponentially,
    // so a sample count a little above n + |x| is already exact to double precision.
    const int N = std::max (64, static_cast<int> (std::ceil (std::abs (x) + n)) * 2 + 32);
    double s = 0.0;
    for (int k = 0; k < N; ++k)
    {
        const double t = 2.0 * kPi * k / N;
        s += std::cos (n * t - x * std::sin (t));
    }
    return s / N;
}

// Associated Legendre P_l^m(x), m >= 0 (Condon-Shortley phase omitted; only shapes matter).
static double legendre (int l, int m, double x)
{
    double pmm = 1.0;
    const double s = std::sqrt (std::max (0.0, 1.0 - x * x));
    for (int i = 1; i <= m; ++i) pmm *= (2.0 * i - 1.0) * s;
    if (l == m) return pmm;
    double pmm1 = x * (2.0 * m + 1.0) * pmm;
    if (l == m + 1) return pmm1;
    double pll = 0.0;
    for (int ll = m + 2; ll <= l; ++ll)
    {
        pll = ((2.0 * ll - 1.0) * x * pmm1 - (ll + m - 1.0) * pmm) / (ll - m);
        pmm = pmm1;
        pmm1 = pll;
    }
    return pll;
}

static double realSphericalHarmonic (int l, int m, Vec3 d)
{
    const double z = clampT (static_cast<double> (d.z), -1.0, 1.0);
    const double phi = std::atan2 (static_cast<double> (d.y), static_cast<double> (d.x));
    const int am = std::abs (m);
    const double p = legendre (l, am, z);
    if (m > 0) return p * std::cos (am * phi);
    if (m < 0) return p * std::sin (am * phi);
    return p;
}

// ------------------------------------------------------------------ geometry
constexpr float kViolinHalfWidth = 0.62f;
constexpr float kCubeHalf        = 0.57735f;     // corners on the unit sphere
constexpr float kIcosaInradius   = 0.79465f;     // circumradius 1

static bool inEllipse (float x, float y, float cx, float cy, float rx, float ry)
{
    const float dx = (x - cx) / rx, dy = (y - cy) / ry;
    return dx * dx + dy * dy < 1.0f;
}

bool insidePlate (Body b, float x, float y)
{
    switch (b)
    {
        case Body::Square:  return std::abs (x) <= 1.0f && std::abs (y) <= 1.0f;
        case Body::Circle:  return x * x + y * y <= 1.0f;
        case Body::Hexagon:
        {
            const float ax = std::abs (x), ay = std::abs (y);
            return ay <= 0.8660254f && 1.7320508f * ax + ay <= 1.7320508f;
        }
        case Body::Violin:
        {
            // Lower bout (wide), upper bout, waist; minus the two C-bouts.
            const bool body = inEllipse (x, y, 0.0f, -0.42f, kViolinHalfWidth, 0.54f)
                           || inEllipse (x, y, 0.0f, 0.47f, 0.50f, 0.46f)
                           || (std::abs (x) < 0.42f && std::abs (y) < 0.25f);
            const float c = 0.07f;
            const bool cut = (x - 0.64f) * (x - 0.64f) + (y - 0.03f) * (y - 0.03f) < c
                          || (x + 0.64f) * (x + 0.64f) + (y - 0.03f) * (y - 0.03f) < c;
            return body && ! cut;
        }
        default: return false;
    }
}

static const std::array<Vec3, 20>& icosaFaceNormals()
{
    // The face normals of an icosahedron point at the vertices of a dodecahedron.
    static const std::array<Vec3, 20> n = []
    {
        const float p = 1.6180339f, q = 1.0f / p;
        std::array<Vec3, 20> v {};
        int k = 0;
        for (int sx : { -1, 1 }) for (int sy : { -1, 1 }) for (int sz : { -1, 1 })
            v[static_cast<size_t> (k++)] = Vec3 (static_cast<float> (sx), static_cast<float> (sy), static_cast<float> (sz)).normalised();
        for (int s1 : { -1, 1 }) for (int s2 : { -1, 1 })
        {
            v[static_cast<size_t> (k++)] = Vec3 (0.0f, s1 * q, s2 * p).normalised();
            v[static_cast<size_t> (k++)] = Vec3 (s1 * q, s2 * p, 0.0f).normalised();
            v[static_cast<size_t> (k++)] = Vec3 (s1 * p, 0.0f, s2 * q).normalised();
        }
        return v;
    }();
    return n;
}

Vec3 surfacePoint (Body b, Vec3 dir)
{
    const Vec3 d = dir.normalised();
    switch (b)
    {
        case Body::Cube:
        {
            const float m = std::max ({ std::abs (d.x), std::abs (d.y), std::abs (d.z) });
            return d * (kCubeHalf / m);
        }
        case Body::Icosahedron:
        {
            float m = 0.0f;
            for (const auto& n : icosaFaceNormals()) m = std::max (m, n.dot (d));
            return d * (kIcosaInradius / m);
        }
        default: return d;
    }
}

Vec3 strikePoint (Body b, float sx, float sy)
{
    sx = clampT (sx, -1.0f, 1.0f);
    sy = clampT (sy, -1.0f, 1.0f);
    if (is3D (b))
    {
        const float az = sx * static_cast<float> (kPi), el = sy * static_cast<float> (kPi) * 0.47f;
        return Vec3 (std::cos (el) * std::sin (az), std::sin (el), std::cos (el) * std::cos (az));
    }
    const float wx = b == Body::Violin ? kViolinHalfWidth : 1.0f;
    Vec3 p (sx * 0.96f * wx, sy * 0.96f, 0.0f);
    for (int i = 0; i < 60 && ! insidePlate (b, p.x, p.y); ++i) p = p * 0.95f;
    return p;
}

void strikeFromDirection (Vec3 d, float& sx, float& sy)
{
    d = d.normalised();
    sx = clampT (std::atan2 (d.x, d.z) / static_cast<float> (kPi), -1.0f, 1.0f);
    sy = clampT (std::asin (clampT (d.y, -1.0f, 1.0f)) / (static_cast<float> (kPi) * 0.47f), -1.0f, 1.0f);
}

Vec3 projectBody (Vec3 p, float yaw, float pitch)
{
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    const float x = p.x * cy + p.z * sy, z0 = -p.x * sy + p.z * cy;
    const float y = p.y * cp - z0 * sp, z = p.y * sp + z0 * cp;
    const float f = 3.2f / (3.2f - z * 0.9f), k = 0.7f;
    return { x * f * k, y * f * k, clampT ((z + 1.0f) * 0.5f, 0.0f, 1.0f) };
}

bool pickSurface (Body b, float tx, float ty, float yaw, float pitch, Vec3& direction)
{
    auto miss = [&] (Vec3 d, float& depth)
    {
        const Vec3 p = projectBody (surfacePoint (b, d), yaw, pitch);
        depth = p.z;
        return std::hypot (p.x - tx, p.y - ty);
    };
    // Coarse scan of directions: among those landing near the target, the nearest to the viewer.
    float best = 1e9f, bestDepth = -1.0f;
    Vec3 bestDir;
    const int NT = 48, NP = 96;
    for (int it = 0; it < NT; ++it)
        for (int ip = 0; ip < NP; ++ip)
        {
            const float th = (it + 0.5f) / NT * static_cast<float> (kPi), ph = ip / static_cast<float> (NP) * 2.0f * static_cast<float> (kPi);
            const Vec3 d (std::sin (th) * std::cos (ph), std::cos (th), std::sin (th) * std::sin (ph));
            float depth;
            const float dist = miss (d, depth);
            if (dist < 0.08f && (depth > bestDepth + 0.05f || (depth > bestDepth - 0.05f && dist < best)))
            {
                best = dist; bestDepth = std::max (bestDepth, depth); bestDir = d;
            }
        }
    if (bestDepth < 0.0f) return false;
    // Then a local search that stays on the front side.
    float step = 0.04f, bestD = 0.0f;
    best = miss (bestDir, bestD);
    for (int iter = 0; iter < 200 && step > 1e-4f; ++iter)
    {
        bool improved = false;
        for (const Vec3 off : { Vec3 (step, 0, 0), Vec3 (-step, 0, 0), Vec3 (0, step, 0), Vec3 (0, -step, 0), Vec3 (0, 0, step), Vec3 (0, 0, -step) })
        {
            const Vec3 d = (bestDir + off).normalised();
            float depth;
            const float dist = miss (d, depth);
            if (dist < best && depth > bestD - 0.1f) { best = dist; bestDir = d; bestD = depth; improved = true; }
        }
        if (! improved) step *= 0.5f;
    }
    direction = bestDir;
    return best < 0.01f;
}

void pickupPoints (Body b, Vec3 strike, float spread, Vec3& left, Vec3& right)
{
    spread = clampT (spread, 0.0f, 1.0f);
    if (is3D (b))
    {
        // Rotate the strike direction about the vertical axis: opposite side, fanned out.
        auto rotY = [] (Vec3 v, float a) { return Vec3 (v.x * std::cos (a) + v.z * std::sin (a), v.y, -v.x * std::sin (a) + v.z * std::cos (a)); };
        const float half = spread * static_cast<float> (kPi) * 0.5f;
        left  = rotY (strike, static_cast<float> (kPi) + half).normalised();
        right = rotY (strike, static_cast<float> (kPi) - half).normalised();
        return;
    }
    float a = std::atan2 (strike.y, strike.x) + static_cast<float> (kPi);
    if (strike.x * strike.x + strike.y * strike.y < 0.0025f) a = static_cast<float> (kPi) * 0.75f;
    const float half = spread * static_cast<float> (kPi) * 0.5f, r = 0.58f;
    const float wx = b == Body::Violin ? kViolinHalfWidth : 1.0f;
    auto place = [&] (float ang)
    {
        Vec3 p (std::cos (ang) * r * wx, std::sin (ang) * r, 0.0f);
        for (int i = 0; i < 60 && ! insidePlate (b, p.x, p.y); ++i) p = p * 0.95f;
        return p;
    };
    left  = place (a + half);
    right = place (a - half);
}


bool isPlate (Body b) { return b == Body::Square || b == Body::Circle || b == Body::Hexagon || b == Body::Violin; }

static const plate::Table& plateTable (Body b)
{
    switch (b)
    {
        case Body::Circle:  return plate::circle;
        case Body::Hexagon: return plate::hexagon;
        case Body::Violin:  return plate::violin;
        default:            return plate::square;
    }
}

// sum_ij c_ij p_i(x / sx) p_j(y / sy) with normalised Legendre polynomials
static double plateShape (Body b, int index, float x, float y)
{
    const auto& t = plateTable (b);
    constexpr int N = plate::kBasis;
    double px[N], py[N];
    const double u = x / t.sx, v = y / t.sy;
    px[0] = py[0] = 1.0;
    px[1] = u; py[1] = v;
    for (int n = 1; n + 1 < N; ++n)
    {
        px[n + 1] = ((2 * n + 1) * u * px[n] - n * px[n - 1]) / (n + 1);
        py[n + 1] = ((2 * n + 1) * v * py[n] - n * py[n - 1]) / (n + 1);
    }
    for (int n = 0; n < N; ++n)
    {
        const double s = std::sqrt ((2.0 * n + 1.0) * 0.5);
        px[n] *= s; py[n] *= s;
    }
    const float* c = t.coeffs + static_cast<size_t> (index) * N * N;
    double sum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        double row = 0.0;
        for (int j = 0; j < N; ++j) row += c[i * N + j] * py[j];
        sum += px[i] * row;
    }
    return sum;
}

static double rawShape (Body b, const Mode& m, Vec3 p)
{
    if (m.plate >= 0)
        return insidePlate (b, p.x, p.y) ? plateShape (b, m.plate, p.x, p.y) : 0.0;

    switch (b)
    {
        case Body::Sphere:
        case Body::Icosahedron:
            return realSphericalHarmonic (m.a, m.b, p.normalised());
        case Body::Cube:
        {
            const Vec3 s = surfacePoint (Body::Cube, p);
            const double ux = s.x / kCubeHalf, uy = s.y / kCubeHalf, uz = s.z / kCubeHalf;
            return std::cos (m.a * kPi * (ux + 1.0) * 0.5) * std::cos (m.b * kPi * (uy + 1.0) * 0.5)
                 * std::cos (m.c * kPi * (uz + 1.0) * 0.5);
        }
        default: return 0.0;
    }
}

float modeShape (Body b, const Mode& m, Vec3 p)
{
    return static_cast<float> (rawShape (b, m, p)) * m.norm;
}

// ------------------------------------------------------------------ mode tables
static std::vector<Mode> enumerate (Body b)
{
    std::vector<Mode> v;
    auto add = [&] (int a, int bb, int c, int variant, double ratio)
    {
        Mode m; m.a = a; m.b = bb; m.c = c; m.variant = variant; m.ratio = ratio;
        v.push_back (m);
    };

    if (isPlate (b))
    {
        const auto& t = plateTable (b);
        for (int k = 0; k < plate::kCount; ++k)
        {
            const auto& info = t.modes[k];
            Mode m;
            m.plate = k;
            m.omega = info.omega;
            m.variant = info.variant;
            m.label = info.label;
            m.a = info.n >= 0 ? info.n : info.group;
            m.b = info.n >= 0 ? info.rings : 0;
            // a perfect plate's degenerate pair rings at one pitch; real plates split it a little
            m.ratio = info.omega * (info.variant ? 1.003 : 1.0);
            v.push_back (m);
        }
    }
    else
    switch (b)
    {
        // the plates come from PlateModes.inc above
        case Body::Sphere:
        case Body::Icosahedron:
        {
            // Shell model: degree l sets the pitch; orders m split slightly (a real
            // sphere is never perfect), much more on the faceted icosahedron.
            // l = 1 is the free shell moving as a whole (like the circle's tilt), so the
            // first vibration is l = 2, the "rugby ball" of a singing bowl or bell.
            const bool ico = b == Body::Icosahedron;
            for (int l = 2; l <= 7; ++l)
                for (int m = -l; m <= l; ++m)
                {
                    const double base = std::pow (l * (l + 1.0), ico ? 0.8 : 0.75);
                    const double split = ico ? 1.0 + 0.018 * m + 0.006 * (m * m) / (l + 1.0) : 1.0 + 0.004 * m;
                    add (l, m, 0, 0, base * split);
                }
            break;
        }
        case Body::Cube:
            for (int a = 0; a <= 4; ++a)
                for (int c = 0; c <= 4; ++c)
                    for (int d = 0; d <= 4; ++d)
                    {
                        if (a == 0 && c == 0 && d == 0) continue;
                        const double k = a * a + c * c + d * d;
                        add (a, c, d, 0, k * (1.0 + 0.003 * (a - d) + 0.0012 * (c - d)));
                    }
            break;
        default: break;
    }

    std::stable_sort (v.begin(), v.end(), [] (const Mode& x, const Mode& y) { return x.ratio < y.ratio; });
    if (v.size() > static_cast<size_t> (kModeTable)) v.resize (kModeTable);
    const double r0 = v.front().ratio;
    for (auto& m : v) m.ratio /= r0;
    return v;
}

static void finalise (Body b, std::vector<Mode>& modes)
{
    for (auto& m : modes)
    {
        if (m.plate >= 0) { m.norm = 1.0f; continue; }   // the generator already normalised the plates

        // Normalise so the shape peaks at +-1 over the body.
        double mx = 1e-12;
        if (is3D (b))
        {
            const int N = 3000;
            const double ga = kPi * (3.0 - std::sqrt (5.0));
            for (int i = 0; i < N; ++i)
            {
                const double y = 1.0 - 2.0 * (i + 0.5) / N, r = std::sqrt (1.0 - y * y), t = ga * i;
                mx = std::max (mx, std::abs (rawShape (b, m, Vec3 (static_cast<float> (r * std::cos (t)), static_cast<float> (y),
                                                                    static_cast<float> (r * std::sin (t))))));
            }
        }
        else
        {
            const int G = 121;
            for (int iy = 0; iy < G; ++iy)
                for (int ix = 0; ix < G; ++ix)
                {
                    const float x = -1.0f + 2.0f * ix / (G - 1), y = -1.0f + 2.0f * iy / (G - 1);
                    mx = std::max (mx, std::abs (rawShape (b, m, Vec3 (x, y, 0.0f))));
                }
        }
        m.norm = static_cast<float> (1.0 / mx);
    }
}

const ModeSet& modeSet (Body b)
{
    static const std::array<ModeSet, kNumBodies> sets = []
    {
        std::array<ModeSet, kNumBodies> s {};
        for (int i = 0; i < kNumBodies; ++i)
        {
            s[static_cast<size_t> (i)].body = static_cast<Body> (i);
            s[static_cast<size_t> (i)].modes = enumerate (static_cast<Body> (i));
            finalise (static_cast<Body> (i), s[static_cast<size_t> (i)].modes);
        }
        return s;
    }();
    return sets[static_cast<size_t> (clampT (static_cast<int> (b), 0, kNumBodies - 1))];
}

std::string modeLabel (Body b, const Mode& m)
{
    if (m.label != nullptr) return m.label;
    const char* dot = "\xC2\xB7";
    char buf[64];
    switch (b)
    {
        case Body::Square:
        case Body::Hexagon:
            std::snprintf (buf, sizeof (buf), "N %d %s M %d %s", m.a, dot, m.b, m.variant == 0 ? "+" : "\xE2\x88\x92");
            break;
        case Body::Violin:
            std::snprintf (buf, sizeof (buf), "N %d %s M %d", m.a, dot, m.b);
            break;
        case Body::Circle:
            std::snprintf (buf, sizeof (buf), "%d diam %s %d ring%s", m.a, dot, m.b - (m.a == 0 ? 0 : 1), (m.b - (m.a == 0 ? 0 : 1)) == 1 ? "" : "s");
            break;
        case Body::Sphere:
        case Body::Icosahedron:
            std::snprintf (buf, sizeof (buf), "\xE2\x84\x93 %d %s m %d", m.a, dot, m.b);
            break;
        case Body::Cube:
            std::snprintf (buf, sizeof (buf), "N %d %s M %d %s L %d", m.a, dot, m.b, dot, m.c);
            break;
        default: buf[0] = 0; break;
    }
    return buf;
}

} // namespace dy::nodal
