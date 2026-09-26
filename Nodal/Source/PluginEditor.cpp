#include "PluginEditor.h"

namespace dy::nodal {

NodalEditor::NodalEditor (NodalProcessor& p)
    : AudioProcessorEditor (p), mixAttachment (p.apvts, "mix", mixKnob)
{
    mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (mixKnob);
    setSize (720, 440);
}

void NodalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0e13));
    g.setColour (juce::Colour (0xffe7b867));
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("DY NODAL", 20, 16, 300, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff7d8797));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("v" DY_VERSION_STRING "  -  skeleton: audio passes through, resonator engine next",
                20, 46, 600, 20, juce::Justification::centredLeft);
}

void NodalEditor::resized()
{
    mixKnob.setBounds (getWidth() / 2 - 50, getHeight() / 2 - 50, 100, 120);
}

} // namespace dy::nodal
