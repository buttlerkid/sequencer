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

// The camera picture with the lips drawn over it.
class CameraView : public juce::Component
{
public:
    explicit CameraView (MouthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    MouthProcessor& proc;
};

// The four mouth shapes as bars, and the note threshold on Open.
class ShapeMeters : public juce::Component
{
public:
    explicit ShapeMeters (MouthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    MouthProcessor& proc;
};

// One output: which shape, where it goes, how it is shaped.
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
    Combo source, dest;
    Knob cc, smooth, lo, hi;
    Toggle invert;
    float shown = -1.0f;
};

class MouthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MouthEditor (MouthProcessor&);
    ~MouthEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 1040, kH = 772;

private:
    void timerCallback() override;
    void fillCameras();
    MouthProcessor& proc;
    dy::nodal::NodalLookAndFeel lnf;
    juce::Component content;
    CameraView camera;
    ShapeMeters meters;
    juce::ComboBox cameraBox;
    juce::TextButton neutral { "Neutral" }, mirror { "Mirror" };
    Knob note, threshold, velocity, level;
    Toggle trigger, speed, preview;
    Combo channel;
    std::array<std::unique_ptr<OutputStrip>, 4> strips;
    juce::Rectangle<int> panels[4];
    juce::String headerText;
    int ticks = 0;
};

} // namespace dy::mouth
