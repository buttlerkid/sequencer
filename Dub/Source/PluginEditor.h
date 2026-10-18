#pragma once
#include "PluginProcessor.h"
#include "NodalLookAndFeel.h"
#include "StepGrid.h"
#include "EnvEditor.h"

namespace dy::dub {

using APVTS = juce::AudioProcessorValueTreeState;
namespace Colours = dy::nodal::Colours;

// The modulation sources' names and colours (LFO 1-3, follower).
const char* sourceName (int i);
juce::Colour sourceColour (int i);
const juce::String& sourceTargetId (int i);
const juce::String& sourceAmountId (int i);

// A slider that hands right-clicks to its owner instead of starting a drag.
class ModSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; } Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

// Rotary + caption + value. Knobs with a modulation target take an LFO or the
// follower from their right-click menu and show a ring where it has moved them.
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& id, juce::String caption, int modTarget = ModOff);
    void resized() override;
    void paint (juce::Graphics&) override;
    void updateModulation (const EngineTelemetry&);
    ModSlider slider;
    const int target;
    std::function<void (int source)> onAssigned;
private:
    void showMenu();
    APVTS& state;
    juce::String caption;
    std::unique_ptr<APVTS::SliderAttachment> att;
    float shown = -2.0f;
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

// A horizontal slider with its caption on the left.
class HSlider : public juce::Component
{
public:
    HSlider (APVTS& s, const juce::String& id, juce::String caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String caption;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// Segmented buttons for a choice parameter.
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (APVTS& s, const juce::String& id, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    int current = 0;
    juce::ParameterAttachment attachment;
};

// A rolling trace of one modulation source.
class Scope : public juce::Component
{
public:
    Scope (const EngineTelemetry& t, int src) : tel (t), source (src) {}
    void push();
    void paint (juce::Graphics&) override;
private:
    const EngineTelemetry& tel;
    int source;
    std::array<float, 96> hist {};
    int head = 0;
};

// One modulation source's controls.
class SourcePage : public juce::Component
{
public:
    SourcePage (DubProcessor& p, int source);
    void resized() override;
    void tick();
    Scope scope;
private:
    int source;
    APVTS& state;
    std::unique_ptr<Combo> shape, target, div;
    std::unique_ptr<Toggle> sync;
    std::unique_ptr<Knob> rate, amount, attack, release;
};

class DubEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DubEditor (DubProcessor&);
    ~DubEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 1180, kH = 830;

private:
    void timerCallback() override;
    void setBank (int b);
    void setSource (int s);
    void panel (juce::Graphics&, juce::Rectangle<int>, const char* title) const;

    DubProcessor& proc;
    dy::nodal::NodalLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Component content;

    juce::ComboBox preset;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    juce::TextButton bankA { "A" }, bankB { "B" }, copyBank { "Copy A > B" };
    Combo rate, pattern, run;
    HSlider length, swing;
    StepGrid grid;
    EnvEditor env;

    ChoiceRow filterMode, revType;
    Knob cutoff, res, envDepth;
    Combo dlyTime;
    Toggle pingPong;
    Knob dlyFb, dlyHp, dlyLp, dlyLevel;
    Knob revSize, revDecay, revTone, revLevel;
    Knob pan, level, mix, output;
    std::array<juce::TextButton, kNumSources> srcTabs;
    std::vector<std::unique_ptr<SourcePage>> pages;

    juce::Rectangle<int> rSeq, rGrid, rEnv, rFilter, rDelay, rReverb, rMod, rOut;
    int viewBank = 0, viewSource = 0, shownVersion = -1, shownPreset = -1;
};

} // namespace dy::dub
