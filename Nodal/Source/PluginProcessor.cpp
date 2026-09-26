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

// ------------------------------------------------------------------ presets
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p = {
        { "Steel Plate", {} },
        { "Brass Bowl", { { "body", 1 }, { "material", 1 }, { "tuneMode", 0 }, { "pitch", 45 }, { "decay", 6 }, { "damping", 0.1f },
                          { "brightness", -0.2f }, { "density", 24 }, { "strikeX", 0.55f }, { "strikeY", 0.1f }, { "mix", 0.6f } } },
        { "Glass Harmonica", { { "body", 1 }, { "material", 2 }, { "tuneMode", 2 }, { "pitch", 60 }, { "decay", 4 }, { "damping", 0.05f },
                               { "brightness", 0.3f }, { "density", 16 }, { "strikeX", 0.7f }, { "strikeY", 0.2f } } },
        { "Gamelan Slendro", { { "body", 2 }, { "material", 1 }, { "scale", 13 }, { "tuneMode", 1 }, { "pitch", 50 }, { "decay", 3.5f },
                               { "damping", 0.3f }, { "brightness", 0.1f }, { "density", 18 }, { "strikeX", 0.3f }, { "strikeY", 0.45f } } },
        { "Violin Top", { { "body", 3 }, { "material", 5 }, { "tuneMode", 0 }, { "pitch", 55 }, { "decay", 1.2f }, { "damping", 0.4f },
                          { "mix", 0.5f }, { "strikeX", 0.25f }, { "strikeY", -0.45f }, { "density", 24 } } },
        { "Crystal Shimmer", { { "body", 1 }, { "material", 3 }, { "scale", 8 }, { "pitch", 67 }, { "decay", 9 }, { "damping", 0.0f },
                               { "brightness", 0.4f }, { "density", 28 }, { "lfo1Target", 5 }, { "lfo1Amount", 0.6f }, { "lfo1Rate", 0.07f },
                               { "lfo1Shape", 5 }, { "lfo2Target", 7 }, { "lfo2Amount", 0.4f }, { "lfo2Rate", 0.13f } } },
        { "Stepping Bell", { { "body", 1 }, { "material", 0 }, { "scale", 9 }, { "pitch", 57 }, { "decay", 3 }, { "glide", 30 },
                             { "lfo1Target", 1 }, { "lfo1Amount", 0.35f }, { "lfo1Sync", 1 }, { "lfo1Div", 7 }, { "lfo1Shape", 4 } } },
        { "Ducked Plate", { { "material", 4 }, { "decay", 1.8f }, { "envTarget", 9 }, { "envAmount", -0.7f }, { "envAttack", 5 },
                            { "envRelease", 400 }, { "mix", 0.8f } } },
        { "Dark Gong", { { "body", 1 }, { "material", 1 }, { "tuneMode", 0 }, { "pitch", 28 }, { "decay", 14 }, { "brightness", -0.6f },
                         { "damping", 0.15f }, { "density", 32 }, { "drive", 3 } } },
        { "Tuned Drum Room", { { "material", 4 }, { "scale", 2 }, { "pitch", 45 }, { "decay", 0.7f }, { "damping", 0.5f },
                               { "envTarget", 4 }, { "envAmount", 0.6f }, { "lowCut", 60 } } },
        { "Follow the Singer", { { "body", 1 }, { "material", 2 }, { "trackPitch", 1 }, { "tuneMode", 2 }, { "decay", 2.5f },
                                 { "mix", 0.45f }, { "glide", 60 } } },
        { "Singing Sphere", { { "body", 4 }, { "material", 2 }, { "tuneMode", 2 }, { "pitch", 57 }, { "decay", 6 }, { "damping", 0.1f },
                              { "brightness", 0.2f }, { "density", 24 }, { "mix", 0.6f }, { "strikeX", 0.2f }, { "strikeY", 0.3f } } },
        { "Icosa Gamelan", { { "body", 6 }, { "material", 1 }, { "scale", 14 }, { "tuneMode", 1 }, { "pitch", 52 }, { "decay", 4 },
                             { "damping", 0.25f }, { "density", 28 }, { "strikeX", -0.3f }, { "strikeY", 0.4f } } },
        { "Crystal Cube", { { "body", 5 }, { "material", 3 }, { "scale", 8 }, { "pitch", 60 }, { "decay", 7 }, { "density", 28 },
                            { "brightness", 0.3f }, { "lfo1Target", 5 }, { "lfo1Amount", 0.5f }, { "lfo1Rate", 0.05f }, { "lfo1Shape", 5 } } },
        { "Orbiting Strike", { { "body", 4 }, { "material", 0 }, { "tuneMode", 0 }, { "pitch", 50 }, { "decay", 5 }, { "damping", 0.15f },
                               { "density", 24 }, { "strikeX", 0.0f }, { "strikeY", 0.25f }, { "lfo1Target", 5 }, { "lfo1Amount", 1.0f },
                               { "lfo1Shape", 2 }, { "lfo1Rate", 0.1f } } },
    };
    return p;
}

void NodalProcessor::loadPreset (int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= static_cast<int> (all.size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            float v = r->getDefaultValue();
            for (const auto& kv : all[static_cast<size_t> (index)].values)
                if (r->paramID == kv.first) v = r->convertTo0to1 (static_cast<float> (kv.second));
            r->beginChangeGesture();
            r->setValueNotifyingHost (v);
            r->endChangeGesture();
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

    TransportInfo t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.valid = true; }
            t.playing = pos->getIsPlaying();
        }
    engine.setTransport (t);

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
    telemetry.lfo1.store (engine.lfoValue (0), std::memory_order_relaxed);
    telemetry.lfo2.store (engine.lfoValue (1), std::memory_order_relaxed);
    telemetry.env.store (engine.envValue(), std::memory_order_relaxed);
    telemetry.heardNote.store (static_cast<float> (engine.heardNote()), std::memory_order_relaxed);
    telemetry.clarity.store (engine.heardClarity(), std::memory_order_relaxed);
    for (int i = 0; i < kNumModTargets; ++i)
        telemetry.mod[static_cast<size_t> (i)].store (engine.modValue (i), std::memory_order_relaxed);
    if (const int hits = engine.takeOnsets(); hits > 0)
        telemetry.strikes.fetch_add (static_cast<uint32_t> (hits), std::memory_order_relaxed);
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
    ui.setProperty ("preset", currentPreset, nullptr);
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
        currentPreset = static_cast<int> (ui.getProperty ("preset", 0));
    }
}

juce::AudioProcessorEditor* NodalProcessor::createEditor() { return new NodalEditor (*this); }

} // namespace dy::nodal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::nodal::NodalProcessor(); }
