#pragma once
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include <vector>

namespace dy::nodal {

// Shared view state between the overlay (message thread) and the renderer (GL thread).
struct PlateViewState
{
    std::atomic<bool>  busy { true };           // renderer: something is moving, keep frames coming
    std::atomic<int>   view { 0 };            // 0 sand, 1 lines, 2 field
    std::atomic<int>   grains { 9000 };
    std::atomic<float> yaw { 0.6f }, pitch { 0.35f };
    std::atomic<bool>  userRotating { false };
    std::atomic<int>   shownMode { 0 };       // the mode the sand is mostly showing (for the HUD)
    std::atomic<int>   shownExtra { 0 };      // other distinct modes ringing at the same pitch
    std::atomic<bool>  shownZonal { false };  // a whole shell multiplet: rings around the strike point
    // plate area in editor pixels (logical, before the GL rendering scale)
    std::atomic<int>   x { 0 }, y { 0 }, w { 0 }, h { 0 }, editorH { 1 };
};

// Projection of a 3D body point into plate units ([-1, 1] square), with depth 0..1 (1 = front).
juce::Point<float> projectBody (Vec3 p, float yaw, float pitch, float& depth);

// Runs on the GL thread: the sand simulation driven by the audio engine's mode
// energies, plus drawing (grains, nodal-line / field textures, outlines).
class PlateRenderer
{
public:
    PlateRenderer (NodalProcessor& p, PlateViewState& s) : proc (p), state (s) {}

    void create (juce::OpenGLContext&);
    void render (juce::OpenGLContext&);
    void release (juce::OpenGLContext&);

private:
    void rebuild (int body);
    void seed();
    void presettle (int iterations);
    bool updateWeights();          // true if anything changed
    void buildTarget (const float* energy, int active);
    void showSingleMode (int index);
    void normaliseField();
    void stepGrains (float shake, bool sprinkle = true);
    float sample2 (float x, float y) const;
    float sample3 (Vec3 d) const;
    void buildTexture();

    NodalProcessor& proc;
    PlateViewState& state;

    std::unique_ptr<juce::OpenGLShaderProgram> points, lines, quad;
    juce::uint32 vao = 0, vbo = 0, tex = 0;

    int body = -1;
    bool three = false;
    static constexpr int G = 128;             // 2D field grid
    static constexpr int LW = 128, LH = 64;   // 3D lat-long grid
    std::vector<float> table, V, Vt, comb;
    std::vector<juce::uint8> inside;          // 2D: grid cell lies on the plate
    float maxV = 1.0f;
    int grains = 0, lastView = -1;
    std::vector<float> px, py, pz;
    std::vector<float> vertexData, lineData;
    std::vector<juce::uint8> texData;
    std::vector<float> outline;               // 2D plate outline in plate units (x, y pairs)
    juce::uint32 lastStrikes = 0;
    float kick = 0.0f;
    bool vertexDirty = true;
    Rng rng { 1234u };
};

// The component that sits over the GL-drawn plate: HUD, markers and mouse.
class PlateView : public juce::Component, private juce::Timer
{
public:
    PlateView (NodalProcessor& p, PlateViewState& s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float>) const;
    juce::Point<float> strikeScreen (bool& visible, bool modulated = false) const;
    float modulatedStrike (int axis) const;
    bool  strikeModulated() const;
    bool  pickSurface (juce::Point<float> screen, Vec3& direction) const;

    NodalProcessor& proc;
    PlateViewState& state;
    bool draggingStrike = false, dragged = false;
    juce::Point<float> downPos;
    float downYaw = 0, downPitch = 0;
    juce::String lastHud;
};

} // namespace dy::nodal
