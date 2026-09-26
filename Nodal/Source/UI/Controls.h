#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace dy::nodal {

using APVTS = juce::AudioProcessorValueTreeState;

// Rounded panel with an optional caption, drawn behind a group of controls.
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String title = {}) : caption (std::move (title)) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    juce::String caption;
};

// Rotary knob + caption + value, attached to a parameter.
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, bool big = false);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;
private:
    juce::String caption;
    bool big;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// A row of buttons for a choice parameter. `shown` picks which choices appear
// (all by default); `icon` optionally draws a glyph above each label.
class ChoiceButtons : public juce::Component
{
public:
    using IconFn = std::function<void (juce::Graphics&, juce::Rectangle<float>, int choice, bool on)>;

    ChoiceButtons (APVTS& state, const juce::String& paramId, std::vector<int> shown = {}, IconFn icon = nullptr);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    std::function<void (int)> onChange;

private:
    int indexAt (juce::Point<float>) const;
    juce::Rectangle<float> cell (int i) const;

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    std::vector<int> choices;
    IconFn icon;
    int current = 0, hover = -1;
    juce::ParameterAttachment attachment;
};

// On/off pill for a bool parameter.
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (APVTS& state, const juce::String& paramId, juce::String onText = "On", juce::String offText = "Off");
private:
    juce::String on, off;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

class ParamCombo : public juce::Component
{
public:
    ParamCombo (APVTS& state, const juce::String& paramId, const juce::String& caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::ComboBox box;
private:
    juce::String caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// Horizontal peak meters (IN / OUT).
class Meters : public juce::Component
{
public:
    explicit Meters (NodalProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    NodalProcessor& proc;
    float in = 0, out = 0;
};

// Mode frequencies drawn against the keyboard of the current key / scale; bar height
// is how strongly each mode rings right now.
class Spectrum : public juce::Component
{
public:
    explicit Spectrum (NodalProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    NodalProcessor& proc;
    std::array<float, kMaxModes> smooth {};
public:
    bool tick();                   // repaints if anything visible changed
private:
    std::array<float, kMaxModes> shown {};
    int shownKey = -1, shownScale = -1;
};

} // namespace dy::nodal
