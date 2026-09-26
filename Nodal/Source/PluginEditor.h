#pragma once
#include "PluginProcessor.h"
#include "UI/Controls.h"
#include "UI/NodalLookAndFeel.h"
#include "UI/PlateView.h"

namespace dy::nodal {

// Laid out at a fixed logical size and scaled with a transform. The OpenGL context
// is attached to the whole editor: the plate is drawn by PlateRenderer in GL, and
// the JUCE components are composited on top (the editor leaves the plate area
// unpainted so the GL image shows through).
class NodalEditor : public juce::AudioProcessorEditor,
                    private juce::OpenGLRenderer,
                    private juce::Timer
{
public:
    explicit NodalEditor (NodalProcessor&);
    ~NodalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kW = 1180, kH = 740;

private:
    void newOpenGLContextCreated() override { renderer.create (gl); }
    void renderOpenGL() override { renderer.render (gl); }
    void openGLContextClosing() override { renderer.release (gl); }
    void timerCallback() override;
    void layout();
    void setView (int v);

    NodalProcessor& proc;
    NodalLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    juce::OpenGLContext gl;
    PlateViewState viewState;
    PlateRenderer renderer;

    juce::Component content;
    Panel tuningPanel { "Body size" }, resonancePanel { "Resonance" }, ioPanel { "Input / output" };

    Knob pitch, glide, lock, density, decay, damping, brightness, spread, drive, lowCut, highCut, mix, output;
    ParamCombo key, scale;
    ParamToggle snap;
    ChoiceButtons tuneMode, bodies, materials;
    juce::TextButton viewSand { "Sand" }, viewLines { "Lines" }, viewField { "Field" };
    juce::Label bodyInfo, snapLabel, tuneCaption, tuneHelp;
    PlateView plate;
    Spectrum spectrum;
    Meters meters;
    juce::Rectangle<int> plateRect;
    int tick = 0, lastGlKey = -1;   // editor coordinates, for the background hole

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodalEditor)
};

} // namespace dy::nodal
