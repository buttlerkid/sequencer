#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/DubEngine.h"

namespace dy::dub {

namespace PID
{
    inline const juce::String run = "run", rate = "rate", swing = "swing", length = "length", pattern = "pattern",
        filterMode = "filterMode", cutoff = "cutoff", res = "res", level = "level", pan = "pan", envDepth = "envDepth",
        dlyTime = "dlyTime", dlyFb = "dlyFb", dlyHp = "dlyHp", dlyLp = "dlyLp", dlyLevel = "dlyLevel", pingPong = "pingPong",
        revType = "revType", revSize = "revSize", revDecay = "revDecay", revTone = "revTone", revLevel = "revLevel",
        mix = "mix", output = "output",
        folAttack = "folAttack", folRelease = "folRelease", folTarget = "folTarget", folAmount = "folAmount";

    inline juce::String lfo (int i, const char* what) { return "lfo" + juce::String (i + 1) + what; }   // Rate Sync Div Shape Target Amount

    // the parameter behind each modulation target
    inline const juce::String& forTarget (int t)
    {
        static const juce::String none;
        static const juce::String* ids[kNumTargets] = { &none, &cutoff, &res, &level, &pan, &envDepth, &dlyFb, &dlyLevel, &revSize, &revLevel, &mix };
        return *ids[clampT (t, 0, kNumTargets - 1)];
    }
}

// The 32 steps, shared between the editor (writes) and the audio thread (reads each block).
class StepStore
{
public:
    enum Field : int { On = 0, Vol, Cut, Res, Pan, Prob, FDepth, Chain, Dly, Rev, VRise, VFall, VSus, VCurve, FRise, FFall, FSus, FCurve, kNumFields };
    static const char* fieldName (int f);

    StepStore() { reset(); }
    void reset();
    float get (int bank, int step, int f) const { return v[idx (bank, step, f)].load (std::memory_order_relaxed); }
    void  set (int bank, int step, int f, float x) { v[idx (bank, step, f)].store (x, std::memory_order_relaxed); ++version; }
    Step  getStep (int bank, int step) const;
    void  setStep (int bank, int step, const Step&);
    void  read (Banks&) const;

    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);

    std::atomic<int> version { 0 };          // bumped on every change (the editor repaints)

private:
    static size_t idx (int b, int s, int f) { return static_cast<size_t> ((clampT (b, 0, kBanks - 1) * kSteps + clampT (s, 0, kSteps - 1)) * kNumFields + f); }
    std::array<std::atomic<float>, kBanks * kSteps * kNumFields> v;
};

class DubProcessor : public juce::AudioProcessor
{
public:
    DubProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    GlobalParams readParams() const;
    juce::AudioProcessorValueTreeState apvts;
    StepStore steps;
    const EngineTelemetry& telemetry() const { return engine.telemetry; }
    std::atomic<double> shownBpm { 120.0 };
    std::atomic<bool> hostSynced { false };
    float uiScale = 0.8f;
    int currentPreset = 0;

private:
    DubEngine engine;
    Banks banks {};
    double freePpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DubProcessor)
};

// Factory presets (Presets.cpp).
juce::StringArray presetNames();
void applyPreset (int index, juce::AudioProcessorValueTreeState&, StepStore&);

} // namespace dy::dub
