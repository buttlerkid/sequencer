#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy::nodal {

juce::AudioProcessor::BusesProperties NodalProcessor::makeBuses()
{
    using Set = juce::AudioChannelSet;
    if constexpr (kInstrumentBuild)
        return BusesProperties().withOutput ("Output", Set::stereo(), true);
    return BusesProperties().withInput ("Input", Set::stereo(), true)
                             .withOutput ("Output", Set::stereo(), true)
                             .withInput ("Sidechain", Set::stereo(), false);
}

NodalProcessor::NodalProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto& v : telemetry.voiceNote) v.store (-1);
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
    using Set = juce::AudioChannelSet;
    const auto& out = layouts.getMainOutputChannelSet();
    const bool okOut = out == Set::stereo() || out == Set::mono();
    if constexpr (kInstrumentBuild) return okOut && layouts.inputBuses.isEmpty();
    const auto& in = layouts.getMainInputChannelSet();
    if (! okOut || ! (in == out || in.isDisabled())) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.inputBuses.getReference (1);
        return sc.isDisabled() || sc == Set::mono() || sc == Set::stereo();
    }
    return true;
}

void NodalProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        if (params.playMode->load() > 0.5f) telemetry.strikes.fetch_add (1, std::memory_order_relaxed);   // throw the sand
    }
    else if (m.isNoteOff())            engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())     engine.sustain (true);
    else if (m.isSustainPedalOff())    engine.sustain (false);
    else if (m.isPitchWheel())         engine.pitchBend (static_cast<float> (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
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
        // ---- v0.4: played from MIDI
        { "Mallet Bells", { { "playMode", 2 }, { "exciter", 0 }, { "body", 1 }, { "material", 1 }, { "tuneMode", 0 }, { "decay", 4 },
                            { "damping", 0.15f }, { "density", 24 }, { "excTone", 0.55f }, { "noteDamp", 0.2f }, { "mix", 1.0f } } },
        { "Plucked Plate", { { "playMode", 2 }, { "exciter", 1 }, { "material", 0 }, { "tuneMode", 2 }, { "decay", 2 }, { "damping", 0.35f },
                             { "density", 20 }, { "excTone", 0.7f }, { "noteDamp", 0.6f }, { "mix", 1.0f }, { "strikeX", 0.7f }, { "strikeY", 0.6f } } },
        { "Bowed Glass", { { "playMode", 2 }, { "exciter", 2 }, { "body", 4 }, { "material", 2 }, { "tuneMode", 2 }, { "decay", 5 },
                           { "damping", 0.1f }, { "density", 20 }, { "excAttack", 180 }, { "excRelease", 600 }, { "excTone", 0.45f },
                           { "noteDamp", 0.3f }, { "mix", 1.0f }, { "lfo2Target", 3 }, { "lfo2Amount", 0.25f }, { "lfo2Rate", 0.2f } } },
        { "Breathing Bowl", { { "playMode", 2 }, { "exciter", 3 }, { "body", 1 }, { "material", 1 }, { "tuneMode", 1 }, { "decay", 6 },
                              { "excAttack", 400 }, { "excRelease", 1200 }, { "excTone", 0.3f }, { "noteDamp", 0.1f }, { "polyphony", 6 },
                              { "mix", 1.0f }, { "brightness", -0.3f } } },
        { "Mono Glide Gong", { { "playMode", 2 }, { "exciter", 0 }, { "body", 6 }, { "material", 1 }, { "tuneMode", 0 }, { "decay", 8 },
                               { "polyphony", 1 }, { "glide", 180 }, { "density", 28 }, { "excTone", 0.35f }, { "mix", 1.0f } } },
        { "Key Follow Drone", { { "playMode", 1 }, { "material", 3 }, { "body", 2 }, { "decay", 10 }, { "damping", 0.05f }, { "glide", 250 },
                                { "snap", 0 }, { "density", 28 }, { "mix", 0.6f } } },
        { "Sidechain Ring", { { "sidechain", 1 }, { "material", 0 }, { "body", 0 }, { "decay", 3 }, { "tuneMode", 1 }, { "mix", 0.5f },
                              { "lowCut", 80 } } },
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

void NodalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getMainBusNumInputChannels(), outCh = getMainBusNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch)
        buffer.clear (ch, 0, n);

    // sidechain (effect build only): the second input bus, if the host connected it
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scL = sc.getReadPointer (0);
                scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
            }
        }
    telemetry.sidechainConnected.store (scL != nullptr, std::memory_order_relaxed);
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

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
    // split the block at every MIDI event so notes start on their exact sample
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            engine.process (L + pos, R + pos, scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
        engine.process (L + pos, R + pos, scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr, n - pos);

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
    for (int v = 0; v < kMaxVoices; ++v)
    {
        telemetry.voiceNote[static_cast<size_t> (v)].store (engine.voiceNote (v), std::memory_order_relaxed);
        telemetry.voiceLevel[static_cast<size_t> (v)].store (engine.voiceLevel (v), std::memory_order_relaxed);
        telemetry.voiceHeld[static_cast<size_t> (v)].store (engine.voiceHeld (v), std::memory_order_relaxed);
    }
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
    ui.setProperty ("tab", uiTab, nullptr);
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
        uiTab = juce::jlimit (0, 1, static_cast<int> (ui.getProperty ("tab", 0)));
    }
}

juce::AudioProcessorEditor* NodalProcessor::createEditor() { return new NodalEditor (*this); }

} // namespace dy::nodal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::nodal::NodalProcessor(); }
