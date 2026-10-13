#pragma once
#include "PluginProcessor.h"
#include "NodalLookAndFeel.h"

namespace dy::pendulum {

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

// The pendulums seen from above: one row per pendulum, the bob's swing left / right.
class WaveView : public juce::Component
{
public:
    explicit WaveView (PendulumProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    double ppq = 0.0;
private:
    PendulumProcessor& proc;
};

// Where we are in the cycle, and how close the pendulums are to lining up.
class CycleBar : public juce::Component
{
public:
    explicit CycleBar (PendulumProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    double ppq = 0.0;
private:
    PendulumProcessor& proc;
};

class PendulumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PendulumEditor (PendulumProcessor&);
    ~PendulumEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 1100, kH = 752;

private:
    void timerCallback() override;
    PendulumProcessor& proc;
    dy::nodal::NodalLookAndFeel lnf;
    juce::Component content;
    WaveView wave;
    CycleBar cycle;
    Knob count, swings, root, step, length, velocity, accent, level;
    Combo cycleBars, trigger, quantize, scale, channel, run, noteMode, layers;
    Toggle descending, preview, layerChannels;
    juce::Rectangle<int> panels[4];
    double shownPpq = 0.0;
    double lastUpdate = 0.0, lastTelemetryPpq = -1.0;
};

} // namespace dy::pendulum
