#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Engine/Generator.h"

namespace dy {

// Parameter controls and generators for the selected track.
class TrackPanel : public juce::Component
{
public:
    explicit TrackPanel (DYSequencerProcessor& p);

    void setTrack (int i);
    void refresh();   // enable/disable pitch controls etc.

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onPatternChanged;   // generators / paste touched step data
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

    DYSequencerProcessor& proc;
    Generator generator;
    int track = 0;

    Knob  steps, pulses, rotate, transpose, fixedNote, swingAmount;
    Combo division, euclidMode, pitchMode, channel, swingMode, swingProfile;

    juce::TextButton rndSteps { "Rnd Steps" }, rndNotes { "Rnd Notes" },
                     arpUp { "Arp Up" }, arpDown { "Arp Down" }, arpUpDown { "Arp Up/Dn" }, arpRandom { "Arp Rnd" },
                     clearBtn { "Clear" }, copyBtn { "Copy" }, pasteBtn { "Paste" };
};

} // namespace dy
