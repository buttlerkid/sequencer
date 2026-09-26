#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy::nodal {

NodalProcessor::NodalProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    params.bind (apvts);
    (void) modeSet (Body::Square);        // build every body's mode tables now, not on the audio thread
    engine.prepare (48000.0);
}

void NodalProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool NodalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    return okOut && (in == out || in.isDisabled());
}

void NodalProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void NodalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear (ch, 0, n);

    engine.setParams (params.read());

    const float s = pendingStrike.exchange (-1.0f);
    if (s >= 0.0f)
    {
        engine.strike (s);
        telemetry.strikes.fetch_add (1, std::memory_order_relaxed);
    }

    float* L = buffer.getWritePointer (0);
    float* R = outCh > 1 ? buffer.getWritePointer (1) : L;
    engine.process (L, R, n);

    // ---- publish for the UI
    const float* e = engine.modeEnergies();
    for (int i = 0; i < kMaxModes; ++i)
    {
        telemetry.energy[static_cast<size_t> (i)].store (e[i], std::memory_order_relaxed);
        telemetry.freq[static_cast<size_t> (i)].store (static_cast<float> (engine.modeFrequency (i)), std::memory_order_relaxed);
    }
    telemetry.body.store (static_cast<int> (std::lround (params.body->load())), std::memory_order_relaxed);
    telemetry.active.store (engine.activeModes(), std::memory_order_relaxed);
    telemetry.dominant.store (engine.dominantMode(), std::memory_order_relaxed);
    telemetry.note.store (static_cast<float> (engine.currentNote()), std::memory_order_relaxed);
    telemetry.f0.store (static_cast<float> (engine.currentF0()), std::memory_order_relaxed);
    float pin = 0, pout = 0;
    engine.takePeaks (pin, pout);
    telemetry.peakIn.store (std::max (pin, telemetry.peakIn.load() * 0.9f), std::memory_order_relaxed);
    telemetry.peakOut.store (std::max (pout, telemetry.peakOut.load() * 0.9f), std::memory_order_relaxed);
}

void NodalProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DYNodal");
    root.setProperty ("version", 1, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    juce::ValueTree ui ("UI");
    ui.setProperty ("view", uiView, nullptr);
    ui.setProperty ("grains", uiGrains, nullptr);
    ui.setProperty ("scale", uiScale, nullptr);
    root.addChild (ui, -1, nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void NodalProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.isValid()) return;

    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid()) apvts.replaceState (p);

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
    {
        uiView   = juce::jlimit (0, 2, static_cast<int> (ui.getProperty ("view", 0)));
        uiGrains = juce::jlimit (2000, 20000, static_cast<int> (ui.getProperty ("grains", 9000)));
        uiScale  = static_cast<float> (static_cast<double> (ui.getProperty ("scale", 0.8)));
    }
}

juce::AudioProcessorEditor* NodalProcessor::createEditor() { return new NodalEditor (*this); }

} // namespace dy::nodal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::nodal::NodalProcessor(); }
