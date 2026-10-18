#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy::dub {

// ------------------------------------------------------------------ steps
const char* StepStore::fieldName (int f)
{
    static const char* n[kNumFields] = { "on", "vol", "cut", "res", "pan", "prob", "fdepth", "chain", "dly", "rev",
                                         "vrise", "vfall", "vsus", "vcurve", "frise", "ffall", "fsus", "fcurve" };
    return n[clampT (f, 0, kNumFields - 1)];
}

void StepStore::reset()
{
    for (int b = 0; b < kBanks; ++b)
        for (int s = 0; s < kSteps; ++s)
            setStep (b, s, Step {});
}

Step StepStore::getStep (int b, int s) const
{
    Step st;
    st.on = get (b, s, On) > 0.5f;
    st.vol = get (b, s, Vol); st.cut = get (b, s, Cut); st.res = get (b, s, Res); st.pan = get (b, s, Pan);
    st.prob = get (b, s, Prob); st.fdepth = get (b, s, FDepth);
    st.chain = get (b, s, Chain) > 0.5f; st.dly = get (b, s, Dly) > 0.5f; st.rev = get (b, s, Rev) > 0.5f;
    st.venv = { get (b, s, VRise), get (b, s, VFall), get (b, s, VSus), get (b, s, VCurve) };
    st.fenv = { get (b, s, FRise), get (b, s, FFall), get (b, s, FSus), get (b, s, FCurve) };
    return st;
}

void StepStore::setStep (int b, int s, const Step& st)
{
    const float vals[kNumFields] = { st.on ? 1.0f : 0.0f, st.vol, st.cut, st.res, st.pan, st.prob, st.fdepth,
                                     st.chain ? 1.0f : 0.0f, st.dly ? 1.0f : 0.0f, st.rev ? 1.0f : 0.0f,
                                     st.venv.rise, st.venv.fall, st.venv.sustain, st.venv.curve,
                                     st.fenv.rise, st.fenv.fall, st.fenv.sustain, st.fenv.curve };
    for (int f = 0; f < kNumFields; ++f) v[idx (b, s, f)].store (vals[f], std::memory_order_relaxed);
    ++version;
}

void StepStore::read (Banks& out) const
{
    for (int b = 0; b < kBanks; ++b)
        for (int s = 0; s < kSteps; ++s)
            out[static_cast<size_t> (b)][static_cast<size_t> (s)] = getStep (b, s);
}

juce::ValueTree StepStore::toTree() const
{
    juce::ValueTree t ("Steps");
    for (int b = 0; b < kBanks; ++b)
    {
        juce::ValueTree bank ("Bank");
        bank.setProperty ("index", b, nullptr);
        for (int f = 0; f < kNumFields; ++f)
        {
            juce::StringArray vals;
            for (int s = 0; s < kSteps; ++s) vals.add (juce::String (get (b, s, f), 4));
            bank.setProperty (fieldName (f), vals.joinIntoString (" "), nullptr);
        }
        t.addChild (bank, -1, nullptr);
    }
    return t;
}

void StepStore::fromTree (const juce::ValueTree& t)
{
    reset();
    for (const auto& bank : t)
    {
        const int b = static_cast<int> (bank.getProperty ("index", 0));
        if (b < 0 || b >= kBanks) continue;
        for (int f = 0; f < kNumFields; ++f)
        {
            if (! bank.hasProperty (fieldName (f))) continue;
            auto vals = juce::StringArray::fromTokens (bank.getProperty (fieldName (f)).toString(), " ", "");
            for (int s = 0; s < kSteps && s < vals.size(); ++s) v[idx (b, s, f)].store (vals[s].getFloatValue());
        }
    }
    ++version;
}

// ------------------------------------------------------------------ parameters
static juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const String& s) { return ParameterID { s, 1 }; };
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bip = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; });
    auto names = [] (const RateDef* d, int n) { StringArray a; for (int i = 0; i < n; ++i) a.add (d[i].name); return a; };
    StringArray patterns, targets, shapes, revs;
    for (auto* p : kPatterns) patterns.add (p);
    for (int t = 0; t < kNumTargets; ++t) targets.add (targetDef (t).name);
    for (int s = 0; s < kNumShapes; ++s) shapes.add (shapeName (s));
    for (int r = 0; r < kNumReverbTypes; ++r) revs.add (reverbName (r));
    const GlobalParams d;

    l.add (std::make_unique<AudioParameterChoice> (id (PID::run), "Run", StringArray { "With transport", "Always" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::rate), "Seq Rate", names (kRates, kNumRates), d.rate));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::swing), "Swing", NormalisableRange<float> (0.0f, 0.5f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (50.0f + v * 50.0f)) + " %"; })));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::length), "Steps", 1, kSteps, kSteps));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::glide), "Glide", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::offset), "Offset", NormalisableRange<float> (-50.0f, 50.0f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::pattern), "Pattern", patterns, 0));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::filterMode), "Filter Type", StringArray { "Low pass", "Band pass", "High pass" }, 0));

    l.add (std::make_unique<AudioParameterFloat>  (id (PID::cutoff), "Cutoff Shift", NormalisableRange<float> (-4.0f, 4.0f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.005f ? "+" : "") + String (v, 2) + " oct"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::res), "Resonance Shift", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::level), "Level", NormalisableRange<float> (0.0f, 1.5f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::pan), "Pan", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                                                       const int p = roundToInt (v * 100.0f); return p == 0 ? String ("C") : String (std::abs (p)) + (p < 0 ? " L" : " R"); })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::envDepth), "Env Depth", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pct));

    l.add (std::make_unique<AudioParameterChoice> (id (PID::dlyTime), "Delay Time", names (kDelayTimes, kNumDelayTimes), d.delayTime));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::dlyFb), "Delay Feedback", NormalisableRange<float> (0.0f, 1.0f), 0.45f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::dlyHp), "Delay High Pass", NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.4f), 300.0f, hz));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::dlyLp), "Delay Tone", NormalisableRange<float> (800.0f, 20000.0f, 0.0f, 0.4f), 6000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::dlyLevel), "Delay Level", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::pingPong), "Ping-Pong", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    l.add (std::make_unique<AudioParameterChoice> (id (PID::revType), "Reverb Type", revs, d.revType));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revSize), "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revDecay), "Reverb Decay", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revTone), "Reverb Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revLevel), "Reverb Level", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revWidth), "Reverb Width", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::revPre), "Reverb Pre-Delay", NormalisableRange<float> (0.0f, 200.0f, 0.0f, 0.5f), 0.0f, ms));

    l.add (std::make_unique<AudioParameterFloat>  (id (PID::mix), "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::output), "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    for (int i = 0; i < kNumLfos; ++i)
    {
        const String n = "LFO " + String (i + 1) + " ";
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::lfo (i, "Rate")), n + "Rate", NormalisableRange<float> (0.02f, 20.0f, 0.0f, 0.3f), 1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; })));
        l.add (std::make_unique<AudioParameterBool>   (id (PID::lfo (i, "Sync")), n + "Sync", true));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfo (i, "Div")), n + "Division", names (kLfoDivs, kNumLfoDivs), 4));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfo (i, "Shape")), n + "Shape", shapes, 0));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfo (i, "Target")), n + "Target", targets, 0));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::lfo (i, "Amount")), n + "Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip));
    }
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::folAttack), "Follower Attack", NormalisableRange<float> (0.5f, 200.0f, 0.0f, 0.4f), 10.0f, ms));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::folRelease), "Follower Release", NormalisableRange<float> (10.0f, 2000.0f, 0.0f, 0.4f), 200.0f, ms));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::folTarget), "Follower Target", targets, 0));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::folAmount), "Follower Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bip));
    return l;
}

DubProcessor::DubProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", layout())
{
    engine.prepare (48000.0);
    // UI tests: DY_DUB_PRESET=<index> starts the plugin on that factory preset
    currentPreset = juce::jlimit (0, presetNames().size() - 1, juce::SystemStats::getEnvironmentVariable ("DY_DUB_PRESET", "0").getIntValue());
    presetName = presetNames()[currentPreset];
    applyPreset (currentPreset, apvts, steps);
}

GlobalParams DubProcessor::readParams() const
{
    auto f = [&] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&] (const juce::String& id) { return static_cast<int> (std::lround (f (id))); };
    GlobalParams p;
    p.runAlways = i (PID::run) == 1;
    p.rate = i (PID::rate); p.swing = f (PID::swing); p.length = i (PID::length); p.pattern = i (PID::pattern);
    p.glide = f (PID::glide); p.offsetMs = f (PID::offset);
    p.filterMode = i (PID::filterMode);
    for (int t = 1; t < kNumTargets; ++t) p.target[static_cast<size_t> (t)] = f (PID::forTarget (t));
    p.delayTime = i (PID::dlyTime); p.delayHpHz = f (PID::dlyHp); p.delayLpHz = f (PID::dlyLp); p.pingPong = f (PID::pingPong);
    p.revType = i (PID::revType); p.revDecay = f (PID::revDecay); p.revTone = f (PID::revTone);
    p.revWidth = f (PID::revWidth); p.revPreMs = f (PID::revPre);
    p.outputDb = f (PID::output);
    for (int k = 0; k < kNumLfos; ++k)
    {
        auto& L = p.lfo[static_cast<size_t> (k)];
        L.rateHz = f (PID::lfo (k, "Rate")); L.sync = f (PID::lfo (k, "Sync")) > 0.5f; L.div = i (PID::lfo (k, "Div"));
        L.shape = i (PID::lfo (k, "Shape")); L.target = i (PID::lfo (k, "Target")); L.amount = f (PID::lfo (k, "Amount"));
    }
    p.follower.attackMs = f (PID::folAttack); p.follower.releaseMs = f (PID::folRelease);
    p.follower.target = i (PID::folTarget); p.follower.amount = f (PID::folAmount);
    return p;
}

void DubProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool DubProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto& in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void DubProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if (outs > 1 && ins == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);         // mono in -> both sides
    for (int c = std::max (ins, 2); c < outs; ++c) buffer.clear (c, 0, n);

    const auto p = readParams();
    engine.setParams (p);
    steps.read (banks);
    engine.setBanks (banks);

    // the song position while the host plays, else our own clock at the host's tempo
    double bpm = 120.0, ppq = freePpq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; }
        }
    const bool run = playing || p.runAlways || wrapperType == wrapperType_Standalone;
    float* L = buffer.getWritePointer (0);
    float* R = outs > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (L, R, n, ppq, bpm, run);
    freePpq = ppq + n * bpm / 60.0 / getSampleRate();

    shownBpm.store (bpm);
    hostSynced.store (playing);
}

// ------------------------------------------------------------------ programs / state
int DubProcessor::getNumPrograms() { return presetNames().size(); }
const juce::String DubProcessor::getProgramName (int i) { return presetNames()[i]; }
void DubProcessor::setCurrentProgram (int i)
{
    if (i < 0 || i >= getNumPrograms()) return;
    currentPreset = i;
    presetName = presetNames()[i];
    applyPreset (i, apvts, steps);
}

void DubProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("DYDub");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("preset", currentPreset, nullptr);
    root.setProperty ("presetName", presetName, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (steps.toTree(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void DubProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid()) apvts.replaceState (p);
    auto s = root.getChildWithName ("Steps");
    if (s.isValid()) steps.fromTree (s);
    uiScale = static_cast<float> (static_cast<double> (root.getProperty ("uiScale", 0.8)));
    currentPreset = static_cast<int> (root.getProperty ("preset", 0));
    presetName = root.getProperty ("presetName", currentPreset >= 0 ? presetNames()[currentPreset] : juce::String()).toString();
}

juce::AudioProcessorEditor* DubProcessor::createEditor() { return new DubEditor (*this); }

} // namespace dy::dub

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::dub::DubProcessor(); }
