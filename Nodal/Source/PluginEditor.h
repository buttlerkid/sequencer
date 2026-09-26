#pragma once
#include "PluginProcessor.h"

namespace dy::nodal {

class NodalEditor : public juce::AudioProcessorEditor
{
public:
    explicit NodalEditor (NodalProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider mixKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment mixAttachment;
};

} // namespace dy::nodal
