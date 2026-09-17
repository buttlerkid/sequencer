#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

// Key / scale, global shuffle, master shift, theme.
class GlobalPanel : public juce::Component
{
public:
    explicit GlobalPanel (DYSequencerProcessor& p);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Combo
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct HSlider
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void setupCombo (Combo&, const juce::String& caption, const juce::StringArray& items, const juce::String& id, const juce::String& tip);
    void setupSlider (HSlider&, const juce::String& caption, const juce::String& id, const juce::String& tip);

    DYSequencerProcessor& proc;
    Combo key, scale, shuffle;
    HSlider swingAmount, masterShift, humanTime, humanVel;
    Combo midiIn;
    juce::Label midiReadout;
};

} // namespace dy
