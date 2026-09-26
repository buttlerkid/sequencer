#include "PlateView.h"
#include "NodalLookAndFeel.h"
#include "Params.h"

namespace dy::nodal {

using namespace juce::gl;

// ------------------------------------------------------------------ projection
juce::Point<float> projectBody (Vec3 p, float yaw, float pitch, float& depth)
{
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    const float x = p.x * cy + p.z * sy, z0 = -p.x * sy + p.z * cy;
    const float y = p.y * cp - z0 * sp, z = p.y * sp + z0 * cp;
    const float f = 3.2f / (3.2f - z * 0.9f), k = 0.7f;
    depth = clampT ((z + 1.0f) * 0.5f, 0.0f, 1.0f);
    return { x * f * k, y * f * k };
}

// ------------------------------------------------------------------ shaders
static const char* kPointVS = R"(
attribute vec4 position;
uniform vec2 scale;
uniform float pointSize;
varying float val;
varying float depth;
void main()
{
    gl_Position = vec4 (position.x * scale.x, position.y * scale.y, 0.0, 1.0);
    val = position.w;
    depth = position.z;
    gl_PointSize = pointSize * (0.75 + 0.5 * position.z);
}
)";

static const char* kPointFS = R"(
varying float val;
varying float depth;
uniform float gain;
uniform int mode;
void main()
{
    vec2 d = gl_PointCoord - vec2 (0.5);
    float r2 = dot (d, d);
    if (r2 > 0.25) discard;
    float soft = 1.0 - r2 * 4.0;
    vec3 brass = vec3 (0.96, 0.80, 0.51);
    vec3 teal  = vec3 (0.49, 0.76, 0.81);
    vec3 c;
    float a;
    if (mode == 0) { float s = 1.0 - smoothstep (0.02, 0.16, val); c = mix (teal, brass, s); a = mix (0.20, 0.85, s); }
    else if (mode == 1) { c = brass; a = exp (-val / 0.02) * 0.9 + 0.03; }
    else { float t = sqrt (clamp (val, 0.0, 1.0)); c = mix (vec3 (0.10, 0.14, 0.20), mix (teal, brass, t), t); a = 0.6; }
    gl_FragColor = vec4 (c * a * soft * (0.30 + 0.70 * depth) * gain, 1.0);
}
)";

static const char* kLineVS = R"(
attribute vec4 position;
uniform vec2 scale;
varying float alpha;
void main()
{
    gl_Position = vec4 (position.x * scale.x, position.y * scale.y, 0.0, 1.0);
    alpha = position.z;
}
)";

static const char* kLineFS = R"(
varying float alpha;
uniform vec4 colour;
void main()
{
    gl_FragColor = vec4 (colour.rgb * colour.a * alpha, 1.0);
}
)";

static const char* kQuadVS = R"(
attribute vec4 position;
uniform vec2 scale;
varying vec2 uv;
void main()
{
    gl_Position = vec4 (position.x * scale.x, position.y * scale.y, 0.0, 1.0);
    uv = position.zw;
}
)";

static const char* kQuadFS = R"(
varying vec2 uv;
uniform sampler2D tex;
void main()
{
    gl_FragColor = texture2D (tex, uv);
}
)";

// Diagnostics for GL problems on users' machines: %TEMP%\DY Nodal GL.log
static void glLog (const juce::String& text)
{
    juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DY Nodal GL.log")
        .appendText (juce::Time::getCurrentTime().toString (true, true) + "  " + text + juce::newLine);
}

static std::unique_ptr<juce::OpenGLShaderProgram> makeProgram (juce::OpenGLContext& ctx, const char* vs, const char* fs)
{
    auto p = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    if (p->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vs))
        && p->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fs))
        && p->link())
        return p;
    glLog ("shader error: " + p->getLastError());
    return nullptr;
}

// ------------------------------------------------------------------ renderer: GL lifecycle
void PlateRenderer::create (juce::OpenGLContext& ctx)
{
    glLog (juce::String ("context: ") + (const char*) glGetString (GL_VERSION) + " / " + (const char*) glGetString (GL_RENDERER));
    points = makeProgram (ctx, kPointVS, kPointFS);
    lines  = makeProgram (ctx, kLineVS, kLineFS);
    quad   = makeProgram (ctx, kQuadVS, kQuadFS);
    glGenVertexArrays (1, &vao);
    glGenBuffers (1, &vbo);
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    body = -1;
}

void PlateRenderer::release (juce::OpenGLContext&)
{
    points.reset(); lines.reset(); quad.reset();
    if (vbo) glDeleteBuffers (1, &vbo);
    if (vao) glDeleteVertexArrays (1, &vao);
    if (tex) glDeleteTextures (1, &tex);
    vbo = vao = tex = 0;
}

// ------------------------------------------------------------------ renderer: simulation
void PlateRenderer::rebuild (int b)
{
    body = clampT (b, 0, kNumBodies - 1);
    const Body bd = static_cast<Body> (body);
    three = is3D (bd);
    const auto& ms = modeSet (bd).modes;

    if (three)
    {
        table.assign (static_cast<size_t> (kMaxModes * LW * LH), 0.0f);
        for (int i = 0; i < kMaxModes; ++i)
            for (int iy = 0; iy < LH; ++iy)
            {
                const float th = (iy + 0.5f) / LH * static_cast<float> (kPi), st = std::sin (th), ct = std::cos (th);
                for (int ix = 0; ix < LW; ++ix)
                {
                    const float ph = ix / static_cast<float> (LW) * 2.0f * static_cast<float> (kPi);
                    table[static_cast<size_t> ((i * LH + iy) * LW + ix)] = modeShape (bd, ms[static_cast<size_t> (i)], Vec3 (st * std::cos (ph), st * std::sin (ph), ct));
                }
            }
        V.assign (static_cast<size_t> (LW * LH), 0.0f);
        inside.clear();
        outline.clear();
    }
    else
    {
        table.assign (static_cast<size_t> (kMaxModes * G * G), 0.0f);
        for (int i = 0; i < kMaxModes; ++i)
            for (int gy = 0; gy < G; ++gy)
                for (int gx = 0; gx < G; ++gx)
                {
                    const float x = gx / float (G - 1) * 2.0f - 1.0f, y = 1.0f - gy / float (G - 1) * 2.0f;
                    table[static_cast<size_t> ((i * G + gy) * G + gx)] = modeShape (bd, ms[static_cast<size_t> (i)], Vec3 (x, y, 0.0f));
                }
        V.assign (static_cast<size_t> (G * G), 0.0f);
        inside.assign (static_cast<size_t> (G * G), 0);
        for (int gy = 0; gy < G; ++gy)
            for (int gx = 0; gx < G; ++gx)
                inside[static_cast<size_t> (gy * G + gx)] = insidePlate (bd, gx / float (G - 1) * 2.0f - 1.0f, 1.0f - gy / float (G - 1) * 2.0f) ? 1 : 0;

        outline.clear();
        const int N = 240;
        for (int k = 0; k < N; ++k)
        {
            const float t = k / float (N) * 2.0f * static_cast<float> (kPi);
            float r = 1.45f;
            while (r > 0.02f && ! insidePlate (bd, r * std::cos (t), r * std::sin (t))) r -= 0.003f;
            outline.push_back (r * std::cos (t));
            outline.push_back (r * std::sin (t));
        }
    }

    // Before anything has been played, show a characteristic figure.
    Vt.assign (V.size(), 0.0f);
    comb.assign (V.size(), 0.0f);
    showSingleMode (5);
    seed();
    presettle (220);
}

void PlateRenderer::seed()
{
    grains = clampT (state.grains.load(), 2000, 20000);
    px.assign (static_cast<size_t> (grains), 0.0f);
    py.assign (static_cast<size_t> (grains), 0.0f);
    pz.assign (static_cast<size_t> (grains), 0.0f);
    const Body bd = static_cast<Body> (body);
    const bool uniform = three && state.view.load() != 0;
    for (int i = 0; i < grains; ++i)
    {
        const size_t k = static_cast<size_t> (i);
        if (three)
        {
            float y, r, t;
            if (uniform) { y = 1.0f - 2.0f * (i + 0.5f) / grains; t = 2.39996f * i; }
            else { y = rng.bipolar(); t = rng.uniform() * 2.0f * static_cast<float> (kPi); }
            r = std::sqrt (std::max (0.0f, 1.0f - y * y));
            px[k] = r * std::cos (t); py[k] = r * std::sin (t); pz[k] = y;
        }
        else
        {
            float x = 0, y = 0;
            for (int tries = 0; tries < 60; ++tries)
            {
                x = rng.bipolar(); y = rng.bipolar();
                if (insidePlate (bd, x, y)) break;
            }
            px[k] = x; py[k] = y;
        }
    }
}

void PlateRenderer::presettle (int iterations)
{
    for (int i = 0; i < iterations; ++i) stepGrains (0.4f, false);
}

bool PlateRenderer::updateWeights()
{
    const int active = clampT (proc.telemetry.active.load(), 0, kMaxModes);
    float e[kMaxModes] {}, tot = 0.0f;
    for (int i = 0; i < active; ++i) { e[i] = proc.telemetry.energy[static_cast<size_t> (i)].load(); tot += e[i]; }
    bool changed = false;
    if (tot > 1e-5f)
    {
        buildTarget (e, active);
        for (size_t k = 0; k < V.size(); ++k) V[k] += 0.2f * (Vt[k] - V[k]);      // morph towards it
        normaliseField();
        changed = true;
    }
    const auto s = proc.telemetry.strikes.load();
    if (s != lastStrikes) { lastStrikes = s; kick = 1.0f; changed = true; }
    return changed;
}

// Modes that ring at (nearly) one frequency move together, so their shapes add up
// coherently: a degenerate pair turns into one figure oriented by the strike point,
// and modes pulled onto the same note make Chladni's hybrid figures. Groups at
// different pitches do not interfere (their cross terms average out), but summing
// them leaves the sand only a few points that are still for all of them, so the
// figure follows the strongest group; close rivals (within ~25 %) blend in by energy^8.
void PlateRenderer::buildTarget (const float* e, int active)
{
    const Body bd = static_cast<Body> (body);
    const auto& ms = modeSet (bd).modes;
    const float sx = clampT (proc.params.strikeX->load() + proc.telemetry.mod[ModStrikeX].load(), -1.0f, 1.0f);
    const float sy = clampT (proc.params.strikeY->load() + proc.telemetry.mod[ModStrikeY].load(), -1.0f, 1.0f);
    const Vec3 sp = strikePoint (bd, sx, sy);

    float emax = 0.0f;
    for (int i = 0; i < active; ++i) emax = std::max (emax, e[i]);
    int idx[kMaxModes], n = 0;
    float f[kMaxModes] {};
    for (int i = 0; i < std::min (active, static_cast<int> (ms.size())); ++i)
    {
        f[i] = proc.telemetry.freq[static_cast<size_t> (i)].load();
        if (e[i] > emax * 1e-4f && f[i] > 0.0f) idx[n++] = i;
    }
    std::sort (idx, idx + n, [&] (int a, int b) { return f[a] < f[b]; });

    struct Group { int first = 0, count = 0; float energy = 0.0f; int loudest = 0; };
    Group groups[kMaxModes];
    int ng = 0;
    for (int k = 0; k < n; ++k)
    {
        const int i = idx[k];
        if (ng == 0 || f[i] > f[idx[k - 1]] * 1.01f) groups[ng++] = { k, 0, 0.0f, i };
        auto& g = groups[ng - 1];
        ++g.count;
        g.energy += e[i];
        if (e[i] > e[g.loudest]) g.loudest = i;
    }
    float gmax = 1e-12f;
    int best = 0;
    for (int g = 0; g < ng; ++g) if (groups[g].energy > gmax) { gmax = groups[g].energy; best = g; }
    if (ng > 0) state.shownMode = groups[best].loudest;

    std::fill (Vt.begin(), Vt.end(), 0.0f);
    const size_t cells = V.size();
    for (int g = 0; g < ng; ++g)
    {
        const float w = std::pow (groups[g].energy / gmax, 8.0f);
        if (w < 0.08f) continue;                      // only close rivals blend in
        std::fill (comb.begin(), comb.end(), 0.0f);
        for (int k = groups[g].first; k < groups[g].first + groups[g].count; ++k)
        {
            const int i = idx[k];
            const float a = (modeShape (bd, ms[static_cast<size_t> (i)], sp) >= 0.0f ? 1.0f : -1.0f) * std::sqrt (e[i]);
            const float* t = table.data() + static_cast<size_t> (i) * cells;
            for (size_t c = 0; c < cells; ++c) comb[c] += a * t[c];
        }
        float m = 1e-20f;
        for (size_t c = 0; c < cells; ++c) { comb[c] *= comb[c]; m = std::max (m, comb[c]); }
        const float s = w / m;
        for (size_t c = 0; c < cells; ++c) Vt[c] += s * comb[c];
    }
    float m = 1e-20f;
    for (float v : Vt) m = std::max (m, v);
    for (auto& v : Vt) v /= m;
}

void PlateRenderer::showSingleMode (int index)
{
    const size_t cells = V.size();
    const float* t = table.data() + static_cast<size_t> (clampT (index, 0, kMaxModes - 1)) * cells;
    for (size_t c = 0; c < cells; ++c) V[c] = t[c] * t[c];
    normaliseField();
    state.shownMode = index;
}

void PlateRenderer::normaliseField()
{
    // Off the plate counts as full vibration, so the edge is not a false nodal line.
    maxV = 1e-6f;
    const bool masked = inside.size() == V.size();
    for (size_t c = 0; c < V.size(); ++c) if (! masked || inside[c]) maxV = std::max (maxV, V[c]);
    if (masked)
        for (size_t c = 0; c < V.size(); ++c) if (! inside[c]) V[c] = maxV;
}

float PlateRenderer::sample2 (float x, float y) const
{
    const float gx = (x + 1.0f) * 0.5f * (G - 1), gy = (1.0f - y) * 0.5f * (G - 1);
    const int ix = static_cast<int> (std::floor (gx)), iy = static_cast<int> (std::floor (gy));
    if (ix < 0 || iy < 0 || ix >= G - 1 || iy >= G - 1) return 1.0f;
    const float fx = gx - ix, fy = gy - iy;
    const size_t i = static_cast<size_t> (iy * G + ix);
    return (V[i] * (1 - fx) * (1 - fy) + V[i + 1] * fx * (1 - fy) + V[i + G] * (1 - fx) * fy + V[i + G + 1] * fx * fy) / maxV;
}

float PlateRenderer::sample3 (Vec3 d) const
{
    const float th = std::acos (clampT (d.z, -1.0f, 1.0f));
    float ph = std::atan2 (d.y, d.x);
    if (ph < 0) ph += 2.0f * static_cast<float> (kPi);
    const float gx = ph / (2.0f * static_cast<float> (kPi)) * LW;
    const float gy = clampT (th / static_cast<float> (kPi) * LH - 0.5f, 0.0f, LH - 1.001f);
    const int ix = static_cast<int> (gx), iy = std::min (static_cast<int> (gy), LH - 2);
    const float fx = gx - ix, fy = clampT (gy - iy, 0.0f, 1.0f);
    const int x0 = ix % LW, x1 = (ix + 1) % LW;
    auto at = [&] (int x, int y) { return V[static_cast<size_t> (y * LW + x)]; };
    return (at (x0, iy) * (1 - fx) * (1 - fy) + at (x1, iy) * fx * (1 - fy) + at (x0, iy + 1) * (1 - fx) * fy + at (x1, iy + 1) * fx * fy) / maxV;
}

void PlateRenderer::stepGrains (float shake, bool sprinkle)
{
    // Sand slides down the time-averaged vibration (sum of w_i * shape_i^2) towards
    // the places that do not move, and is thrown about where the plate moves most.
    const Body bd = static_cast<Body> (body);
    const float settle = 0.028f * std::sqrt (shake);      // quiet plates still sort the sand, just slower
    const float jit = shake + kick * 2.5f;
    if (settle <= 1e-5f && jit <= 1e-4f) return;

    // A trickle of fresh sand while the plate rings: piles left by an earlier figure
    // dissolve into the current one instead of growing for ever.
    const int fresh = sprinkle ? static_cast<int> (grains * 0.0015f * std::min (1.0f, shake * 2.0f) + rng.uniform()) : 0;
    for (int s = 0; s < fresh; ++s)
    {
        const size_t k = static_cast<size_t> (std::min (grains - 1, static_cast<int> (rng.uniform() * grains)));
        if (three)
        {
            const float y = rng.bipolar(), t = rng.uniform() * 2.0f * static_cast<float> (kPi), r = std::sqrt (std::max (0.0f, 1.0f - y * y));
            px[k] = r * std::cos (t); py[k] = r * std::sin (t); pz[k] = y;
        }
        else
            for (int tries = 0; tries < 20; ++tries)
            {
                const float x = rng.bipolar(), y = rng.bipolar();
                if (insidePlate (bd, x, y)) { px[k] = x; py[k] = y; break; }
            }
    }

    for (int i = 0; i < grains; ++i)
    {
        const size_t k = static_cast<size_t> (i);
        if (three)
        {
            const Vec3 d (px[k], py[k], pz[k]);
            const float v = sample3 (d), e = 0.02f;
            Vec3 t1 = std::abs (d.z) < 0.9f ? Vec3 (-d.y, d.x, 0.0f) : Vec3 (0.0f, -d.z, d.y);
            t1 = t1.normalised();
            const Vec3 t2 = d.cross (t1);
            const float g1 = sample3 (d + t1 * e) - v, g2 = sample3 (d + t2 * e) - v;
            const float gl = std::sqrt (g1 * g1 + g2 * g2) + 1e-9f;
            const float sv = std::sqrt (v);
            const float st = settle * std::min (1.0f, 0.2f + sv * 2.5f), j = (0.0008f + 0.035f * sv) * jit;
            const float m1 = -g1 / gl * st + rng.bipolar() * 0.5f * j, m2 = -g2 / gl * st + rng.bipolar() * 0.5f * j;
            const Vec3 n = (d + t1 * m1 + t2 * m2).normalised();
            px[k] = n.x; py[k] = n.y; pz[k] = n.z;
        }
        else
        {
            const float x = px[k], y = py[k], v = sample2 (x, y), e = 2.0f / G;
            const float gx = sample2 (x + e, y) - sample2 (x - e, y), gy = sample2 (x, y + e) - sample2 (x, y - e);
            const float gl = std::sqrt (gx * gx + gy * gy) + 1e-9f;
            const float sv = std::sqrt (v);
            const float st = settle * std::min (1.0f, 0.2f + sv * 2.5f), j = (0.0008f + 0.035f * sv) * jit;
            const float nx = x - gx / gl * st + rng.bipolar() * 0.5f * j;
            const float ny = y - gy / gl * st + rng.bipolar() * 0.5f * j;
            if (insidePlate (bd, nx, ny)) { px[k] = nx; py[k] = ny; }
        }
    }
}

void PlateRenderer::buildTexture()
{
    texData.resize (static_cast<size_t> (G * G * 4));
    const Body bd = static_cast<Body> (body);
    const bool linesView = state.view.load() == 1;
    for (int gy = 0; gy < G; ++gy)
        for (int gx = 0; gx < G; ++gx)
        {
            const size_t k = static_cast<size_t> (gy * G + gx);
            auto* d = texData.data() + k * 4;
            const float x = gx / float (G - 1) * 2.0f - 1.0f, y = 1.0f - gy / float (G - 1) * 2.0f;
            if (! insidePlate (bd, x, y)) { d[0] = d[1] = d[2] = d[3] = 0; continue; }
            const float v = V[k] / maxV;
            float r, g, b, a;
            if (linesView)
            {
                a = std::exp (-v / 0.015f) * 0.95f + 0.05f;
                r = 0.91f * a; g = 0.72f * a; b = 0.40f * a;
            }
            else
            {
                const float t = std::sqrt (v);
                const float cr = 0.49f + (0.91f - 0.49f) * t, cg = 0.76f + (0.72f - 0.76f) * t, cb = 0.81f + (0.40f - 0.81f) * t;
                r = 0.08f + (cr - 0.08f) * t; g = 0.10f + (cg - 0.10f) * t; b = 0.14f + (cb - 0.14f) * t; a = 1.0f;
            }
            d[0] = static_cast<juce::uint8> (clampT (r, 0.0f, 1.0f) * 255); d[1] = static_cast<juce::uint8> (clampT (g, 0.0f, 1.0f) * 255);
            d[2] = static_cast<juce::uint8> (clampT (b, 0.0f, 1.0f) * 255); d[3] = static_cast<juce::uint8> (clampT (a, 0.0f, 1.0f) * 255);
        }
}

// ------------------------------------------------------------------ renderer: frame
void PlateRenderer::render (juce::OpenGLContext& ctx)
{
    const float rs = static_cast<float> (ctx.getRenderingScale());
    const int ew = state.w.load(), eh = state.h.load();
    glClearColor (0.043f, 0.055f, 0.075f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);
    if (ew <= 0 || eh <= 0 || points == nullptr || lines == nullptr || quad == nullptr) return;

    const int vx = juce::roundToInt (state.x.load() * rs);
    const int vy = juce::roundToInt ((state.editorH.load() - state.y.load() - eh) * rs);
    const int vw = juce::roundToInt (ew * rs), vh = juce::roundToInt (eh * rs);

    // ---- simulation
    const int b = proc.telemetry.body.load();
    bool dirty = false;
    if (b != body) { rebuild (b); dirty = true; }
    const int view = state.view.load();
    if (view != lastView || state.grains.load() != grains)
    {
        const bool reseedNeeded = (three && (view == 0) != (lastView == 0)) || state.grains.load() != grains;
        lastView = view;
        if (reseedNeeded) { seed(); if (view == 0) presettle (30); }
        dirty = true;
    }
    dirty |= updateWeights();
    if (three && ! state.userRotating.load())
    {
        state.yaw = state.yaw.load() + 0.0035f;          // slow turntable
        vertexDirty = true;
    }
    float tot = 0.0f;
    for (int i = 0; i < kMaxModes; ++i) tot += proc.telemetry.energy[static_cast<size_t> (i)].load();
    // loudness in dB drives the shaking, so quiet material still sorts its sand
    const float shake = tot > 1e-9f ? clampT ((10.0f * std::log10 (tot) + 60.0f) / 45.0f, 0.0f, 1.0f) : 0.0f;
    const bool moving = shake > 0.002f || kick > 0.01f;
    if (view == 0 && moving) stepGrains (shake);
    if (dirty || moving || state.userRotating.load()) vertexDirty = true;
    kick = kick > 0.01f ? kick * 0.85f : 0.0f;
    state.busy = moving || dirty || three || state.userRotating.load();

    // ---- drawing
    glViewport (vx, vy, vw, vh);
    glEnable (GL_SCISSOR_TEST);
    glScissor (vx, vy, vw, vh);
    glClearColor (0.055f, 0.071f, 0.098f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);
    glEnable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glEnable (GL_PROGRAM_POINT_SIZE);
    // Compatibility contexts (what Windows drivers usually hand out) only fill in
    // gl_PointCoord when point sprites are on; core contexts reject the enum, so
    // clear the error it raises there.
    glEnable (0x8861 /* GL_POINT_SPRITE */);
    (void) glGetError();
    glBindVertexArray (vao);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);

    const float sidePx = std::min (vw, vh) * 0.88f;
    const float sx = sidePx / vw, sy = sidePx / vh;
    const float yaw = state.yaw.load(), pitch = state.pitch.load();

    auto bindPositions = [] (juce::OpenGLShaderProgram& prog)
    {
        const GLint loc = glGetAttribLocation (prog.getProgramID(), "position");
        if (loc >= 0)
        {
            glVertexAttribPointer (static_cast<GLuint> (loc), 4, GL_FLOAT, GL_FALSE, 4 * sizeof (float), nullptr);
            glEnableVertexAttribArray (static_cast<GLuint> (loc));
        }
        return loc;
    };

    // field / nodal-line texture (2D)
    if (! three && view != 0)
    {
        buildTexture();
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, tex);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, G, G, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
        const float q[] = { -1, -1, 0, 1,   1, -1, 1, 1,   -1, 1, 0, 0,   1, 1, 1, 0 };
        glBufferData (GL_ARRAY_BUFFER, sizeof (q), q, GL_STREAM_DRAW);
        quad->use();
        glUniform2f (glGetUniformLocation (quad->getProgramID(), "scale"), sx, sy);
        glUniform1i (glGetUniformLocation (quad->getProgramID(), "tex"), 0);
        glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        const GLint loc = bindPositions (*quad);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        if (loc >= 0) glDisableVertexAttribArray (static_cast<GLuint> (loc));
    }

    // outline (2D) or wireframe (3D)
    lineData.clear();
    if (! three)
    {
        for (size_t i = 0; i + 1 < outline.size(); i += 2)
        {
            const size_t j = (i + 2) % outline.size();
            lineData.insert (lineData.end(), { outline[i], outline[i + 1], 1.0f, 0.0f, outline[j], outline[j + 1], 1.0f, 0.0f });
        }
    }
    else
    {
        const Body bd = static_cast<Body> (body);
        auto seg = [&] (Vec3 a, Vec3 c)
        {
            float da, dc;
            const auto p = projectBody (a, yaw, pitch, da), q = projectBody (c, yaw, pitch, dc);
            lineData.insert (lineData.end(), { p.x, p.y, 0.25f + 0.75f * da, 0.0f, q.x, q.y, 0.25f + 0.75f * dc, 0.0f });
        };
        if (bd == Body::Cube)
        {
            const float h = 0.57735f;
            for (float a : { -h, h }) for (float c : { -h, h })
            {
                seg ({ a, c, -h }, { a, c, h }); seg ({ a, -h, c }, { a, h, c }); seg ({ -h, a, c }, { h, a, c });
            }
        }
        else if (bd == Body::Icosahedron)
        {
            const float p = 1.6180339f;
            std::vector<Vec3> v;
            for (float a : { -1.0f, 1.0f }) for (float c : { -1.0f, 1.0f })
            {
                v.push_back (Vec3 (0, a, c * p)); v.push_back (Vec3 (a, c * p, 0)); v.push_back (Vec3 (a * p, 0, c));
            }
            for (size_t i = 0; i < v.size(); ++i)
                for (size_t j = i + 1; j < v.size(); ++j)
                    if (std::abs ((v[i] - v[j]).length() - 2.0f) < 0.01f) seg (v[i].normalised(), v[j].normalised());
        }
        else
        {
            for (int ring = 0; ring < 3; ++ring)
                for (int s = 0; s < 72; ++s)
                {
                    const float t0 = s / 72.0f * 2.0f * static_cast<float> (kPi), t1 = (s + 1) / 72.0f * 2.0f * static_cast<float> (kPi);
                    auto pt = [ring] (float t) { return ring == 0 ? Vec3 (std::cos (t), std::sin (t), 0) : ring == 1 ? Vec3 (std::cos (t), 0, std::sin (t)) : Vec3 (0, std::cos (t), std::sin (t)); };
                    seg (pt (t0), pt (t1));
                }
        }
    }
    if (! lineData.empty())
    {
        glBufferData (GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (lineData.size() * sizeof (float)), lineData.data(), GL_STREAM_DRAW);
        lines->use();
        glUniform2f (glGetUniformLocation (lines->getProgramID(), "scale"), sx, sy);
        glUniform4f (glGetUniformLocation (lines->getProgramID(), "colour"), 0.85f, 0.87f, 0.90f, three ? 0.22f : 0.30f);
        glBlendFunc (GL_ONE, GL_ONE);
        const GLint loc = bindPositions (*lines);
        glDrawArrays (GL_LINES, 0, static_cast<GLsizei> (lineData.size() / 4));
        if (loc >= 0) glDisableVertexAttribArray (static_cast<GLuint> (loc));
    }

    // grains
    if (three || view == 0)
    {
        const Body bd = static_cast<Body> (body);
        if (vertexDirty || vertexData.size() != static_cast<size_t> (grains * 4))
        {
        vertexData.resize (static_cast<size_t> (grains * 4));
        for (int i = 0; i < grains; ++i)
        {
            const size_t k = static_cast<size_t> (i);
            float* d = vertexData.data() + k * 4;
            if (three)
            {
                const Vec3 dir (px[k], py[k], pz[k]);
                float depth;
                const auto p = projectBody (surfacePoint (bd, dir), yaw, pitch, depth);
                d[0] = p.x; d[1] = p.y; d[2] = depth; d[3] = sample3 (dir);
            }
            else
            {
                d[0] = px[k]; d[1] = py[k]; d[2] = 1.0f; d[3] = sample2 (px[k], py[k]);
            }
        }
        vertexDirty = false;
        }
        glBufferData (GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (vertexData.size() * sizeof (float)), vertexData.data(), GL_STREAM_DRAW);
        points->use();
        glUniform2f (glGetUniformLocation (points->getProgramID(), "scale"), sx, sy);
        glUniform1f (glGetUniformLocation (points->getProgramID(), "pointSize"), std::max (1.6f, sidePx / 330.0f));
        glUniform1f (glGetUniformLocation (points->getProgramID(), "gain"), 1.0f);
        glUniform1i (glGetUniformLocation (points->getProgramID(), "mode"), three ? view : 0);
        glBlendFunc (GL_ONE, GL_ONE);
        const GLint loc = bindPositions (*points);
        glDrawArrays (GL_POINTS, 0, grains);
        if (loc >= 0) glDisableVertexAttribArray (static_cast<GLuint> (loc));
    }

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindVertexArray (0);
    glDisable (GL_SCISSOR_TEST);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

// ------------------------------------------------------------------ overlay component
PlateView::PlateView (NodalProcessor& p, PlateViewState& s) : proc (p), state (s)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

void PlateView::timerCallback()
{
    // Repaint the overlay only when what it shows has changed.
    juce::String key;
    key << proc.telemetry.body.load() << '|' << state.shownMode.load() << '|' << juce::roundToInt (proc.telemetry.f0.load() * 10.0f)
        << '|' << proc.params.material->load() << '|' << proc.params.strikeX->load() << '|' << proc.params.strikeY->load()
        << '|' << proc.params.spread->load() << '|' << state.yaw.load() << '|' << state.pitch.load()
        << '|' << juce::roundToInt (modulatedStrike (0) * 400.0f) << '|' << juce::roundToInt (modulatedStrike (1) * 400.0f)
        << '|' << juce::roundToInt (proc.telemetry.mod[ModSpread].load() * 400.0f) << '|' << juce::roundToInt (proc.telemetry.note.load() * 10.0f);
    if (key != lastHud) { lastHud = key; repaint(); }
}

juce::Point<float> PlateView::toScreen (float x, float y) const
{
    const float side = std::min (getWidth(), getHeight()) * 0.88f;
    return { getWidth() * 0.5f + x * side * 0.5f, getHeight() * 0.5f - y * side * 0.5f };
}

juce::Point<float> PlateView::fromScreen (juce::Point<float> p) const
{
    const float side = std::min (getWidth(), getHeight()) * 0.88f;
    return { (p.x - getWidth() * 0.5f) / (side * 0.5f), (getHeight() * 0.5f - p.y) / (side * 0.5f) };
}

float PlateView::modulatedStrike (int axis) const
{
    const float base = (axis == 0 ? proc.params.strikeX : proc.params.strikeY)->load();
    return clampT (base + proc.telemetry.mod[static_cast<size_t> (axis == 0 ? ModStrikeX : ModStrikeY)].load(), -1.0f, 1.0f);
}

bool PlateView::strikeModulated() const
{
    auto on = [&] (int t)
    {
        const auto& p = proc.params;
        for (int i = 0; i < 2; ++i) if (static_cast<int> (std::lround (p.lfoTarget[i]->load())) == t) return true;
        return static_cast<int> (std::lround (p.envTarget->load())) == t;
    };
    return on (ModStrikeX) || on (ModStrikeY) || on (ModSpread);
}

juce::Point<float> PlateView::strikeScreen (bool& visible, bool modulated) const
{
    const Body bd = static_cast<Body> (clampT (proc.telemetry.body.load(), 0, kNumBodies - 1));
    const Vec3 s = modulated ? strikePoint (bd, modulatedStrike (0), modulatedStrike (1))
                             : strikePoint (bd, proc.params.strikeX->load(), proc.params.strikeY->load());
    visible = true;
    if (! is3D (bd)) return toScreen (s.x, s.y);
    float depth;
    const auto p = projectBody (surfacePoint (bd, s), state.yaw.load(), state.pitch.load(), depth);
    visible = depth > 0.45f;
    return toScreen (p.x, p.y);
}

void PlateView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // round the GL rectangle's corners into the frame colour, then a hairline border
    juce::Path mask;
    mask.addRectangle (r);
    mask.addRoundedRectangle (r, 8.0f);
    mask.setUsingNonZeroWinding (false);
    g.setColour (Colours::frame);
    g.fillPath (mask);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    const Body bd = static_cast<Body> (clampT (proc.telemetry.body.load(), 0, kNumBodies - 1));
    const auto& mat = kMaterials[clampT (static_cast<int> (std::lround (proc.params.material->load())), 0, kNumMaterials - 1)];
    const auto& ms = modeSet (bd).modes;
    const int dom = clampT (state.shownMode.load(), 0, static_cast<int> (ms.size()) - 1);

    // HUD
    auto top = getLocalBounds().reduced (14, 10).removeFromTop (18);
    g.setFont (displayFont (11.0f));
    g.setColour (Colours::brass);
    const juce::String title = juce::String (bodyName (bd)).toUpperCase() + (is3D (bd) ? " SHELL" : " PLATE");
    g.drawText (title, top, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.drawText (juce::String (mat.name).toUpperCase(), top.withTrimmedLeft (static_cast<int> (juce::GlyphArrangement::getStringWidth (displayFont (11.0f), title)) + 14),
                juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.setFont (monoFont (12.0f));
    g.drawText (juce::String::fromUTF8 (modeLabel (bd, ms[static_cast<size_t> (dom)]).c_str()), top, juce::Justification::centredRight);

    auto bottom = getLocalBounds().reduced (14, 10).removeFromBottom (16);
    const float note = proc.telemetry.note.load(), f0 = proc.telemetry.f0.load();
    const float cm = 30.0f * std::sqrt (261.63f / std::max (1.0f, f0));
    g.setColour (Colours::dim);
    g.setFont (monoFont (11.0f));
    g.drawText (juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 3) + "  " + juce::String (f0, 1) + " Hz  "
                    + juce::String::fromUTF8 ("\xE2\x89\x88 ") + juce::String (juce::roundToInt (cm)) + " cm",
                bottom, juce::Justification::centredLeft);
    g.drawText (is3D (bd) ? "drag to turn  click to strike" : "click to strike  drag the dot to move it", bottom, juce::Justification::centredRight);

    // markers
    bool vis = false;
    const bool modded = strikeModulated();
    const auto sp = strikeScreen (vis, modded);
    auto dot = [&] (juce::Point<float> c, juce::Colour col, float rad, const char* txt)
    {
        g.setColour (col);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (Colours::page);
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 2.0f);
        if (txt != nullptr)
        {
            g.setFont (monoFont (rad * 1.3f).boldened());
            g.drawText (txt, juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2), juce::Justification::centred);
        }
    };
    if (! is3D (bd))
    {
        Vec3 l, rr;
        const float spr = clampT (proc.params.spread->load() + proc.telemetry.mod[ModSpread].load(), 0.0f, 1.0f);
        if (modded)
            pickupPoints (bd, strikePoint (bd, modulatedStrike (0), modulatedStrike (1)), spr, l, rr);
        else
            pickupPoints (bd, strikePoint (bd, proc.params.strikeX->load(), proc.params.strikeY->load()), spr, l, rr);
        dot (toScreen (l.x, l.y), Colours::teal, 7.0f, "L");
        dot (toScreen (rr.x, rr.y), Colours::teal, 7.0f, "R");
    }
    if (modded)
    {
        // the set position stays as a hollow handle you can drag; the filled dot is where it is now
        bool v0 = false;
        const auto home = strikeScreen (v0, false);
        if (v0)
        {
            g.setColour (Colours::brass.withAlpha (0.8f));
            g.drawEllipse (home.x - 8.0f, home.y - 8.0f, 16.0f, 16.0f, 1.5f);
            if (vis) { g.setColour (Colours::brass.withAlpha (0.35f)); g.drawLine ({ home, sp }, 1.0f); }
        }
    }
    if (vis) dot (sp, Colours::brass, 8.0f, nullptr);
}

void PlateView::mouseDown (const juce::MouseEvent& e)
{
    downPos = e.position;
    dragged = false;
    downYaw = state.yaw.load();
    downPitch = state.pitch.load();
    const Body bd = static_cast<Body> (clampT (proc.telemetry.body.load(), 0, kNumBodies - 1));
    draggingStrike = false;
    if (! is3D (bd))
    {
        bool vis;
        draggingStrike = strikeScreen (vis).getDistanceFrom (e.position) < 16.0f;
        for (auto* id : { &PID::strikeX, &PID::strikeY })
            if (auto* p = proc.apvts.getParameter (*id)) p->beginChangeGesture();
        if (! draggingStrike) mouseDrag (e);      // click elsewhere: move the strike point there
    }
    else
        state.userRotating = true;
}

void PlateView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3) dragged = true;
    const Body bd = static_cast<Body> (clampT (proc.telemetry.body.load(), 0, kNumBodies - 1));
    if (is3D (bd))
    {
        state.yaw = downYaw + (e.position.x - downPos.x) * 0.01f;
        state.pitch = clampT (downPitch + (e.position.y - downPos.y) * 0.01f, -1.3f, 1.3f);
        return;
    }
    const auto p = fromScreen (e.position);
    const float wx = bd == Body::Violin ? 0.62f : 1.0f;
    auto set = [&] (const juce::String& id, float v)
    {
        if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (clampT (v, -1.0f, 1.0f)));
    };
    set (PID::strikeX, p.x / (0.96f * wx));
    set (PID::strikeY, p.y / 0.96f);
    repaint();
}

void PlateView::mouseUp (const juce::MouseEvent&)
{
    const Body bd = static_cast<Body> (clampT (proc.telemetry.body.load(), 0, kNumBodies - 1));
    if (! is3D (bd))
        for (auto* id : { &PID::strikeX, &PID::strikeY })
            if (auto* p = proc.apvts.getParameter (*id)) p->endChangeGesture();
    state.userRotating = false;
    if (! dragged || ! draggingStrike)
        if (! (is3D (bd) && dragged))
            proc.requestStrike (0.9f);
    draggingStrike = false;
}

void PlateView::mouseMove (const juce::MouseEvent& e)
{
    bool vis;
    setMouseCursor (strikeScreen (vis).getDistanceFrom (e.position) < 16.0f ? juce::MouseCursor::DraggingHandCursor
                                                                              : juce::MouseCursor::CrosshairCursor);
}

} // namespace dy::nodal
