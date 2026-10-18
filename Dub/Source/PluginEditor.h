#pragma once
#include "PluginProcessor.h"
#include "DubLookAndFeel.h"
#include "StepStrips.h"
#include "StepGrid.h"
#include "EnvEditor.h"

namespace dy::dub {

using APVTS = juce::AudioProcessorValueTreeState;

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

// Caption, knob, value. Knobs with a modulation target take an LFO or the follower
// from their right-click menu and show a ring where it has moved them.
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

// Caption above a drop-down (choice and int parameters).
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

// A recessed panel with a bold title, behind a group of controls.
class Section : public juce::Component
{
public:
    explicit Section (juce::String t = {}) : title (std::move (t)) {}
    void paint (juce::Graphics&) override;
    juce::String title;
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
    std::array<float, 120> hist {};
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

// Every source, where it goes and how far, live.
class ModSummary : public juce::Component
{
public:
    explicit ModSummary (DubProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onPick;
private:
    DubProcessor& proc;
};

class DubEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DubEditor (DubProcessor&);
    ~DubEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kW = 1320, kH = 860;

private:
    enum Page { PageAdv = 0, PageMain, PageEnv, PageMod, kNumPages };
    void timerCallback() override;
    void setBank (int b);
    void setPage (int p);
    void setSource (int s);
    void refreshPresets();
    void stepPreset (int delta);
    void savePreset();
    void showPresetName();

    DubProcessor& proc;
    DubLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Component content;

    // header
    juce::ComboBox category, preset;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, save { "Save" }, random { "Random" };
    juce::TextButton bankA { "A" }, bankB { "B" };
    Combo pattern;
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    // strips and the row under them
    StripArea strips;
    juce::TextButton rndCut { "Rnd cut" }, rndRes { "Rnd res" }, rndVol { "Rnd vol" }, rndOn { "Rnd on/off" }, copyBank { "Copy A > B" };
    std::array<juce::TextButton, kNumPages> tabs;
    std::array<juce::Component, kNumPages> pages;

    // MAIN
    Section secSeq { "SEQUENCER" }, secDelay { "DELAY" }, secReverb { "REVERB" };
    Knob cutoff, res, level, swing, output, glide, offset, mix, pan, envDepth2;
    Knob dlyLevel, dlyFb, dlyLp, dlyHp, pingPong;
    Knob revLevel, revSize, revTone, revWidth, revDecay, revPre;
    Combo filterMode, rate, length, run, dlyTime, revType;

    // ENV
    Section secEnv { "ENVELOPES" }, secEnvDepth { "FILTER ENV" };
    EnvEditor env;
    Knob envDepth;

    // MOD
    Section secMod { "MODULATION" }, secRouting { "ROUTING" };
    std::array<juce::TextButton, kNumSources> srcTabs;
    std::vector<std::unique_ptr<SourcePage>> srcPages;
    ModSummary summary;

    // ADV
    Section secAdv;
    StepGrid grid;

    int viewBank = 0, page = PageMain, viewSource = 0, shownVersion = -1;
    juce::String shownName;
};

} // namespace dy::dub
