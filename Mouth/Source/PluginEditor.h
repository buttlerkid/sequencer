#pragma once
#include "PluginProcessor.h"
#include "NodalLookAndFeel.h"

namespace dy::mouth {

using APVTS = juce::AudioProcessorValueTreeState;
namespace Colours = dy::nodal::Colours;

// Rotary + caption + value.
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& id, juce::String caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String caption;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class Combo : public juce::Component
{
public:
    Combo (APVTS& s, const juce::String& id, juce::String caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::ComboBox box;
private:
    juce::String caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS& s, const juce::String& id, juce::String text);
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// The camera picture with the lips, hands and circle trails drawn over it.
class CameraView : public juce::Component
{
public:
    explicit CameraView (MouthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    MouthProcessor& proc;
};

// A grid of bars: some shapes (rows) of some faces or hands (columns), and the note
// threshold on Open.
class ShapeMeters : public juce::Component
{
public:
    ShapeMeters (MouthProcessor& p, int firstSource, int sources, int firstWho, int whos)
        : proc (p), source0 (firstSource), nSources (sources), who0 (firstWho), nWho (whos) {}
    void paint (juce::Graphics&) override;
private:
    MouthProcessor& proc;
    int source0, nSources, who0, nWho;
};

// The take: drag it into Live as a MIDI clip.
class TakeDrag : public juce::Component
{
public:
    explicit TakeDrag (MouthProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    MouthProcessor& proc;
    bool dragging = false;
};

// One output: whose, which shape, where it goes, how it is shaped.
class OutputStrip : public juce::Component
{
public:
    OutputStrip (MouthProcessor& p, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    MouthProcessor& proc;
    int index;
    Combo who, source, dest;
    Knob cc, smooth, lo, hi;
    Toggle invert;
    float shown = -1.0f;
    int shownWho = -1;
};

class MouthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MouthEditor (MouthProcessor&);
    ~MouthEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 1250, kH = 802;

private:
    void timerCallback() override;
    void fillCameras();
    MouthProcessor& proc;
    dy::nodal::NodalLookAndFeel lnf;
    juce::Component content;
    CameraView camera;
    ShapeMeters faceMeters, handMeters;
    juce::ComboBox cameraBox;
    juce::TextButton neutral { "Neutral" }, mirror { "Mirror" }, record { "Record" };
    Knob note, threshold, velocity, level, circleNote, circleLength;
    Toggle trigger, speed, preview, faceChannels, circleNotes;
    Combo channel, pitchMode, chord, scale, faces, hands, handMode;
    TakeDrag take;
    std::array<std::unique_ptr<OutputStrip>, 4> strips;
    juce::Rectangle<int> panels[6];
    juce::String headerText;
    int ticks = 0;
};

} // namespace dy::mouth
