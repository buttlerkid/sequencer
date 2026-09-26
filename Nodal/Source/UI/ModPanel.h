#pragma once
#include "Controls.h"
#include "NodalLookAndFeel.h"

namespace dy::nodal {

// A scrolling trace of a modulation source over the last few seconds.
class Scope : public juce::Component
{
public:
    Scope (juce::Colour c, bool isBipolar) : colour (c), bipolar (isBipolar) { setInterceptsMouseClicks (false, false); }
    void push (float v, bool isActive);     // call at the UI rate (30 Hz)
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    bool bipolar, active = false;
    std::array<float, 120> hist {};
    int pos = 0, still = 0;
};

// One LFO: shape, target, free rate or tempo division, depth.
class LfoSlot : public juce::Component
{
public:
    LfoSlot (NodalProcessor& p, int index, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();

    const juce::Colour accent;

private:
    NodalProcessor& proc;
    const int index;
    Scope scope;
    Knob rate, div, amount;
    ParamCombo shape, target;
    ParamToggle sync;
};

// Envelope follower on the input level, plus pitch following.
class InputSlot : public juce::Component
{
public:
    explicit InputSlot (NodalProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();

    const juce::Colour accent = Colours::lilac;

private:
    NodalProcessor& proc;
    Scope scope;
    Knob attack, release, amount;
    ParamCombo target;
    ParamToggle track;
    juce::Rectangle<int> pitchArea;
    float shownNote = -2.0f, shownClarity = -1.0f;
};

class ModPanel : public Panel
{
public:
    explicit ModPanel (NodalProcessor& p);
    void resized() override;
    void tick();

    LfoSlot lfo1, lfo2;
    InputSlot input;
};

} // namespace dy::nodal
