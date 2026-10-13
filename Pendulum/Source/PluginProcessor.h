#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/PendulumEngine.h"
#include <array>

namespace dy::pendulum {

namespace PID
{
    inline const juce::String count = "count", cycleBars = "cycleBars", swings = "swings", trigger = "trigger",
        quantize = "quantize", root = "root", scale = "scale", step = "step", descending = "descending",
        length = "length", velocity = "velocity", accent = "accent", channel = "channel",
        preview = "preview", previewLevel = "previewLevel", run = "run",
        noteMode = "noteMode", layers = "layers", layerChannels = "layerChannels";
}

// A small polyphonic bell so the plugin is audible on its own (turn it off to play
// another instrument from its MIDI output).
class PreviewSynth
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v = {}; }
    void noteOn (int note, float vel);
    void render (float* L, float* R, int start, int n, float gain);
private:
    struct Voice { double ph1 = 0, ph2 = 0, inc1 = 0, inc2 = 0; float amp = 0, amp2 = 0, decay = 0, decay2 = 0, pan = 0.5f; };
    std::array<Voice, 48> voices {};
    int next = 0;
    double sr = 48000.0;
};

// What the UI needs, published by the audio thread.
struct Telemetry
{
    std::atomic<double> ppq { 0.0 };
    std::atomic<bool>   running { false }, hostSynced { false };
    std::atomic<double> bpm { 120.0 };
};

class PendulumProcessor : public juce::AudioProcessor
{
public:
    PendulumProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    PendulumParams readParams() const;
    juce::AudioProcessorValueTreeState apvts;
    Telemetry telemetry;
    PendulumEngine display;          // a copy of the settings for the editor's drawing (message thread)
    float uiScale = 0.85f;

private:
    PendulumEngine engine;
    PreviewSynth synth;
    std::vector<NoteEvent> events;
    double freePpq = 0.0, lastPpq = -1.0;
    bool wasRunning = false;
    std::array<std::atomic<float>*, 19> raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PendulumProcessor)
};

} // namespace dy::pendulum
