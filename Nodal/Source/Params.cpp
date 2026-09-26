#include "Params.h"

namespace dy::nodal {

juce::StringArray bodyNames()     { juce::StringArray a; for (int i = 0; i < kNumBodies; ++i) a.add (bodyName (static_cast<Body> (i))); return a; }
juce::StringArray materialNames() { juce::StringArray a; for (auto& m : kMaterials) a.add (m.name); return a; }
juce::StringArray scaleNames()    { juce::StringArray a; for (auto& s : kScales) a.add (s.name); return a; }
juce::StringArray keyNames()      { juce::StringArray a; for (auto* k : kKeyNames) a.add (k); return a; }
juce::StringArray tuneModeNames() { return { "Free", "Scale", "Harmonic" }; }
juce::StringArray modTargetNames() { juce::StringArray a; for (int i = 0; i < kNumModTargets; ++i) a.add (modTargetName (i)); return a; }
juce::StringArray lfoShapeNames()  { juce::StringArray a; for (int i = 0; i < kNumLfoShapes; ++i) a.add (lfoShapeName (i)); return a; }
juce::StringArray syncDivNames()   { juce::StringArray a; for (auto& d : kSyncDivs) a.add (d.name); return a; }

static juce::String noteText (float note)
{
    return juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 3);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const String& s) { return ParameterID { s, 1 }; };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    l.add (std::make_unique<AudioParameterChoice> (id (PID::body), "Body", bodyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::material), "Material", materialNames(), 0));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::pitch), "Body Pitch", NormalisableRange<float> (24.0f, 96.0f, 0.01f), 48.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return noteText (v); })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::key), "Key", keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::scale), "Scale", scaleNames(), 1));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::snap), "Snap To Scale", true));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::glide), "Glide", skewed (0.0f, 2000.0f, 200.0f), 80.0f,
                                                   AudioParameterFloatAttributes().withLabel ("ms")
                                                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::tuneMode), "Mode Tuning", tuneModeNames(), TuneScale));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::lock), "Lock", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::density), "Modes", 1, kMaxModes, 20));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::decay), "Decay", skewed (0.05f, 20.0f, 1.5f), 2.5f,
                                                   AudioParameterFloatAttributes().withLabel ("s").withStringFromValueFunction ([] (float v, int)
                                                   { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::damping), "Damping", NormalisableRange<float> (0.0f, 1.0f), 0.2f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::brightness), "Brightness", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                   { return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f)); })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::strikeX), "Strike X", NormalisableRange<float> (-1.0f, 1.0f), 0.42f));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::strikeY), "Strike Y", NormalisableRange<float> (-1.0f, 1.0f), 0.30f));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::spread), "Stereo Spread", NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::lowCut), "Low Cut", skewed (20.0f, 2000.0f, 150.0f), 30.0f,
                                                   AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::highCut), "High Cut", skewed (500.0f, 20000.0f, 4000.0f), 18000.0f,
                                                   AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                                                   { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::drive), "Drive", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 6.0f,
                                                   AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                   { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::mix), "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.7f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::output), "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                   AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                   { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    // ---- modulation
    auto bip = [] (float v, int) { return (v > 0.005f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; };
    const float defRate[2] = { 0.25f, 0.07f };
    for (int i = 0; i < 2; ++i)
    {
        const String n = "LFO " + String (i + 1) + " ";
        l.add (std::make_unique<AudioParameterFloat> (id (PID::lfoRate[i]), n + "Rate", skewed (0.01f, 20.0f, 1.0f), defRate[i],
                                                      AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
                                                      { return v < 1.0f ? String (v, 2) + " Hz" : String (v, 1) + " Hz"; })));
        l.add (std::make_unique<AudioParameterBool>   (id (PID::lfoSync[i]), n + "Sync", false));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfoDiv[i]), n + "Division", syncDivNames(), 3));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfoShape[i]), n + "Shape", lfoShapeNames(), i == 0 ? ShapeSine : ShapeDrift));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::lfoTarget[i]), n + "Target", modTargetNames(), ModOff));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::lfoAmount[i]), n + "Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.3f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (bip)));
    }
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::envAttack), "Envelope Attack", skewed (0.5f, 200.0f, 20.0f), 10.0f,
                                                   AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (v, v < 10 ? 1 : 0) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::envRelease), "Envelope Release", skewed (10.0f, 3000.0f, 300.0f), 250.0f,
                                                   AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::envTarget), "Envelope Target", modTargetNames(), ModOff));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::envAmount), "Envelope Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.5f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction (bip)));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::trackPitch), "Follow Input Pitch", false));
    return l;
}

void ParamRefs::bind (juce::AudioProcessorValueTreeState& s)
{
    auto g = [&] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    body = g (PID::body); material = g (PID::material); pitch = g (PID::pitch); key = g (PID::key); scale = g (PID::scale);
    snap = g (PID::snap); glide = g (PID::glide); tuneMode = g (PID::tuneMode); lock = g (PID::lock); density = g (PID::density);
    decay = g (PID::decay); damping = g (PID::damping); brightness = g (PID::brightness); strikeX = g (PID::strikeX);
    strikeY = g (PID::strikeY); spread = g (PID::spread); lowCut = g (PID::lowCut); highCut = g (PID::highCut);
    drive = g (PID::drive); mix = g (PID::mix); output = g (PID::output);
    for (int i = 0; i < 2; ++i)
    {
        lfoRate[i] = g (PID::lfoRate[i]); lfoSync[i] = g (PID::lfoSync[i]); lfoDiv[i] = g (PID::lfoDiv[i]);
        lfoShape[i] = g (PID::lfoShape[i]); lfoTarget[i] = g (PID::lfoTarget[i]); lfoAmount[i] = g (PID::lfoAmount[i]);
    }
    envAttack = g (PID::envAttack); envRelease = g (PID::envRelease); envTarget = g (PID::envTarget); envAmount = g (PID::envAmount);
    trackPitch = g (PID::trackPitch);
}

EngineParams ParamRefs::read() const
{
    auto i = [] (std::atomic<float>* p) { return static_cast<int> (std::lround (p->load())); };
    auto f = [] (std::atomic<float>* p) { return static_cast<double> (p->load()); };
    EngineParams e;
    e.body = i (body); e.material = i (material); e.pitch = f (pitch); e.key = i (key); e.scale = i (scale);
    e.snap = snap->load() > 0.5f; e.glideMs = f (glide); e.tuneMode = i (tuneMode); e.lock = f (lock);
    e.density = i (density); e.decay = f (decay); e.damping = f (damping); e.brightness = f (brightness);
    e.strikeX = f (strikeX); e.strikeY = f (strikeY); e.spread = f (spread); e.lowCut = f (lowCut); e.highCut = f (highCut);
    e.driveDb = f (drive); e.mix = f (mix); e.outDb = f (output);
    for (int k = 0; k < 2; ++k)
    {
        e.lfo[k].rateHz = f (lfoRate[k]); e.lfo[k].sync = lfoSync[k]->load() > 0.5f; e.lfo[k].div = i (lfoDiv[k]);
        e.lfo[k].shape = i (lfoShape[k]); e.lfo[k].target = i (lfoTarget[k]); e.lfo[k].amount = f (lfoAmount[k]);
    }
    e.env.attackMs = f (envAttack); e.env.releaseMs = f (envRelease); e.env.target = i (envTarget); e.env.amount = f (envAmount);
    e.trackPitch = trackPitch->load() > 0.5f;
    return e;
}

} // namespace dy::nodal
