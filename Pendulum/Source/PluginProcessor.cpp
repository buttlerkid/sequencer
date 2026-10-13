#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy::pendulum {

// ------------------------------------------------------------------ preview bell
void PreviewSynth::noteOn (int note, float vel)
{
    auto& v = voices[static_cast<size_t> (next)];
    next = (next + 1) % static_cast<int> (voices.size());
    const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    v.ph1 = v.ph2 = 0.0;
    v.inc1 = f / sr;
    v.inc2 = f * 2.756 / sr;                                   // a bar's second partial: a soft metallic ping
    v.amp = vel * vel * 0.22f;
    v.amp2 = v.amp * 0.4f;
    const double t60 = 0.5 + 1.6 * std::exp (-(note - 48) / 24.0);     // low notes ring longer
    v.decay  = static_cast<float> (std::exp (-6.9 / (sr * t60)));
    v.decay2 = static_cast<float> (std::exp (-6.9 / (sr * t60 * 0.3)));
    v.pan = 0.5f + 0.35f * static_cast<float> (std::sin (note * 1.7));
}

void PreviewSynth::render (float* L, float* R, int start, int n, float gain)
{
    for (auto& v : voices)
    {
        if (v.amp < 1e-5f) continue;
        const float gl = gain * std::sqrt (1.0f - v.pan), gr = gain * std::sqrt (v.pan);
        for (int s = start; s < start + n; ++s)
        {
            const float x = static_cast<float> (std::sin (2.0 * kPi * v.ph1)) * v.amp + static_cast<float> (std::sin (2.0 * kPi * v.ph2)) * v.amp2;
            v.ph1 += v.inc1; if (v.ph1 >= 1.0) v.ph1 -= 1.0;
            v.ph2 += v.inc2; if (v.ph2 >= 1.0) v.ph2 -= 1.0;
            v.amp *= v.decay;
            v.amp2 *= v.decay2;
            L[s] += x * gl;
            if (R != L) R[s] += x * gr;
        }
    }
}

// ------------------------------------------------------------------ parameters
static juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const String& s) { return ParameterID { s, 1 }; };
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    StringArray trig, grids, scales, chans;
    for (int i = 0; i < kNumTriggers; ++i) trig.add (triggerName (i));
    for (auto& g : kGrids) grids.add (g.name);
    for (auto& s : kScales) scales.add (s.name);
    for (int c = 1; c <= 16; ++c) chans.add (String (c));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::count), "Pendulums", 1, kMaxPendulums, 12));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::cycleBars), "Cycle", StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars" }, 3));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::swings), "Swings", 1, 64, 16,
                                                   AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " / cycle"; })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::trigger), "Trigger", trig, TrigCrossings));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::quantize), "Quantize", grids, 0));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::root), "Root", 24, 96, 48,
                                                   AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 3); })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::scale), "Scale", scales, 9));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::step), "Step", 0, 7, 1,
                                                   AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("unison") : "+" + String (v); })));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::descending), "Long = High", false));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::length), "Length", NormalisableRange<float> (0.03f, 4.0f, 0.0f, 0.4f), 0.25f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " beats"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::velocity), "Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::accent), "Phase Accent", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::channel), "MIDI Channel", chans, 0));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::preview), "Preview Sound", true));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::previewLevel), "Preview Level", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::run), "Run", StringArray { "With transport", "Always" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::noteMode), "Notes", StringArray { "Scale", "Drum pads" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::layers), "Layers", StringArray { "1 layer", "2 layers", "3 layers", "4 layers" }, 0));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::layerChannels), "Layer = Channel", false));
    return l;
}

static const juce::String* kIds[19] = { &PID::count, &PID::cycleBars, &PID::swings, &PID::trigger, &PID::quantize, &PID::root, &PID::scale,
                                        &PID::step, &PID::descending, &PID::length, &PID::velocity, &PID::accent, &PID::channel,
                                        &PID::preview, &PID::previewLevel, &PID::run, &PID::noteMode, &PID::layers, &PID::layerChannels };

PendulumProcessor::PendulumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", layout())
{
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = apvts.getRawParameterValue (*kIds[i]);
    engine.prepare (48000.0);
}

PendulumParams PendulumProcessor::readParams() const
{
    auto i = [&] (int k) { return static_cast<int> (std::lround (raw[static_cast<size_t> (k)]->load())); };
    auto f = [&] (int k) { return raw[static_cast<size_t> (k)]->load(); };
    PendulumParams p;
    p.count = i (0);
    p.cycleBeats = 4.0 * (1 << i (1));
    p.swings = i (2); p.trigger = i (3); p.quantize = i (4); p.root = i (5); p.scale = i (6); p.step = i (7);
    p.descending = f (8) > 0.5f; p.noteBeats = f (9); p.velocity = f (10); p.accent = f (11); p.channel = i (12) + 1;
    p.noteMode = i (16); p.layers = i (17) + 1; p.layerChannels = f (18) > 0.5f;
    return p;
}

void PendulumProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    synth.prepare (sampleRate);
    lastPpq = -1.0;
}

bool PendulumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void PendulumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();                                    // the pendulums are the only source of notes

    const auto p = readParams();
    // a change in the settings that moves notes releases what sounds, so nothing hangs
    const auto before = engine.getParams();
    const bool remap = before.count != p.count || before.root != p.root || before.scale != p.scale || before.step != p.step
                    || before.descending != p.descending || before.channel != p.channel
                    || before.noteMode != p.noteMode || before.layers != p.layers || before.layerChannels != p.layerChannels;
    events.clear();
    if (remap) engine.allNotesOff (0, events);
    engine.setParams (p);

    // where we are: the host's song position while it plays, else our own clock
    double bpm = 120.0, ppq = freePpq;
    bool playing = false, synced = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; synced = true; }
        }
    const bool always = raw[15]->load() > 0.5f;
    const bool run = playing || always || wrapperType == wrapperType_Standalone;
    if (! synced) ppq = freePpq;

    if (run)
    {
        // a jump in the song (loop, relocate) releases the sounding notes
        const double expected = lastPpq;
        if (expected >= 0.0 && std::abs (ppq - expected) > 0.05) engine.allNotesOff (0, events);
        engine.process (ppq, bpm, n, events);
        const double adv = n * bpm / 60.0 / getSampleRate();
        lastPpq = ppq + adv;
        freePpq = synced ? ppq + adv : freePpq + adv;
    }
    else if (wasRunning)
    {
        engine.allNotesOff (0, events);
        lastPpq = -1.0;
    }
    wasRunning = run;

    float* L = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    const bool preview = raw[13]->load() > 0.5f;
    const float gain = raw[14]->load();
    int pos = 0;
    for (const auto& e : events)
    {
        midi.addEvent (e.on ? juce::MidiMessage::noteOn (e.channel, e.note, e.velocity)
                            : juce::MidiMessage::noteOff (e.channel, e.note), e.sample);
        if (preview && L != nullptr && e.on)
        {
            synth.render (L, R, pos, e.sample - pos, gain);
            pos = e.sample;
            synth.noteOn (e.note, e.velocity);
        }
    }
    if (preview && L != nullptr) synth.render (L, R, pos, n - pos, gain);

    telemetry.ppq.store (lastPpq >= 0.0 ? lastPpq : ppq);
    telemetry.running.store (run);
    telemetry.hostSynced.store (synced);
    telemetry.bpm.store (bpm);
}

void PendulumProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("DYPendulum");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void PendulumProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid()) apvts.replaceState (p);
    uiScale = static_cast<float> (static_cast<double> (root.getProperty ("uiScale", 0.85)));
}

juce::AudioProcessorEditor* PendulumProcessor::createEditor() { return new PendulumEditor (*this); }

} // namespace dy::pendulum

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::pendulum::PendulumProcessor(); }
