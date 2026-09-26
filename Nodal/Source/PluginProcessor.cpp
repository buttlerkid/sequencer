#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy::nodal {

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix",
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("%")));
    return layout;
}

NodalProcessor::NodalProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createLayout())
{
    mix = apvts.getRawParameterValue ("mix");
}

void NodalProcessor::prepareToPlay (double, int) {}

bool NodalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && layouts.getMainInputChannelSet() == out;
}

void NodalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    // Pass-through: the wet path (modal resonator) is not built yet.
    juce::ignoreUnused (mix);
}

void NodalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NodalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* NodalProcessor::createEditor() { return new NodalEditor (*this); }

} // namespace dy::nodal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::nodal::NodalProcessor(); }
