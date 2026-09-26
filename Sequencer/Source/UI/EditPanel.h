#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "LaneEditor.h"
#include "Theme.h"
#include "Engine/Generator.h"

namespace dy {

// Everything about the selected track that is not in its overview row:
// settings, lane editor, lane macros, generators.
class EditPanel : public juce::Component
{
public:
    explicit EditPanel (DYSequencerProcessor& p);

    void setTrack (int i);       // -1 = nothing selected
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onPatternChanged;
    std::function<void (const juce::String&)> onHint;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    void setupKnob (Knob&, const juce::String& caption, const juce::String& tip);
    void setupCombo (Combo&, const juce::String& caption, const juce::StringArray& items, const juce::String& tip);
    void addButton (juce::TextButton&, const juce::String& tip, std::function<void()> action);
    void setControlsVisible (bool);

    DYSequencerProcessor& proc;
    Generator generator;
    int track = -1;

    Combo division, euclidMode, pitchMode, channel, swingMode, swingProfile;
    struct Box
    {
        ValueBox    box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    } swingAmount;
    Knob  velMacro, lengthMacro, shiftMacro, probMacro, repsMacro, transposeMacro;

    LaneEditor lanes;

    juce::TextButton rndSteps { "Rnd Steps" }, rndNotes { "Rnd Notes" },
                     arpUp { "Arp Up" }, arpDown { "Arp Down" }, arpUpDown { "Arp Up/Dn" }, arpRandom { "Arp Rnd" },
                     clearBtn { "Clear" }, copyBtn { "Copy" }, pasteBtn { "Paste" };
};

} // namespace dy
