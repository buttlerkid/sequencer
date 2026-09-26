#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/NodalEngine.h"
#include "Params.h"
#include <array>

namespace dy::nodal {

// What the audio thread publishes for the UI (plate view, spectrum, meters).
struct Telemetry
{
    std::array<std::atomic<float>, kMaxModes> energy {};
    std::array<std::atomic<float>, kMaxModes> freq {};
    std::atomic<int>      body { 0 }, active { 0 }, dominant { 0 };
    std::atomic<float>    note { 48.0f }, f0 { 130.8f }, peakIn { 0.0f }, peakOut { 0.0f };
    std::atomic<uint32_t> strikes { 0 };        // bumps on every tap / transient, for the sand "kick"
    std::atomic<float>    lfo1 { 0.0f }, lfo2 { 0.0f }, env { 0.0f }, heardNote { -1.0f }, clarity { 0.0f };
    std::array<std::atomic<float>, kNumModTargets> mod {};     // summed modulation per target
    std::array<std::atomic<int>, kMaxVoices>   voiceNote {};     // -1 idle
    std::array<std::atomic<float>, kMaxVoices> voiceLevel {};
    std::array<std::atomic<bool>, kMaxVoices>  voiceHeld {};
    std::atomic<bool> sidechainConnected { false };
};

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, double>> values;   // everything else goes to its default
};
const std::vector<Preset>& factoryPresets();

class NodalProcessor : public juce::AudioProcessor
{
public:
    NodalProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void handleMidi (const juce::MidiMessage&);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return static_cast<int> (factoryPresets().size()); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override
    {
        return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (factoryPresets()[static_cast<size_t> (index)].name) : juce::String();
    }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Tap-to-strike from the UI (message thread).
    void requestStrike (float velocity) { pendingStrike.store (velocity); }

    // Set a parameter from the UI as one undoable host gesture.
    void setParam (const juce::String& id, float plainValue);
    void loadPreset (int index);
    int  currentPreset = 0;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;     // notes played on the editor's keyboard strip
    ParamRefs params;
    Telemetry telemetry;

    // Editor preferences (saved with the state)
    int   uiView = 0;          // 0 sand, 1 lines, 2 field
    int   uiGrains = 9000;
    int   uiTab = 0;           // bottom panel: 0 modulation, 1 play
    float uiScale = 0.8f;

private:
    static BusesProperties makeBuses();
    NodalEngine engine;
    std::atomic<float> pendingStrike { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodalProcessor)
};

} // namespace dy::nodal
