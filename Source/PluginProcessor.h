#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Pattern.h"
#include "Engine/Sequencer.h"
#include "Params/Parameters.h"
#include <optional>

namespace dy {

// Clipboard payload for one track: step data plus its automatable parameters
// (as normalised values, in trackParamSuffixes() order) and its name.
struct TrackClip
{
    TrackSnapshot steps;
    std::vector<float> params;
    juce::String name;
};

enum class PadLayout : int { GmDrums = 0, ChromaticC1, Melodic };

class DYSequencerProcessor : public juce::AudioProcessor
{
public:
    DYSequencerProcessor();
    ~DYSequencerProcessor() override = default;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- model
    juce::AudioProcessorValueTreeState apvts;
    ParamRefs    params;
    PatternModel pattern;
    Sequencer    sequencer;

    TrackSettings      trackSettings (int i) const { return params.tracks[static_cast<size_t> (i)].read(); }
    // Settings for the audio thread, with solo resolved into mute.
    TrackSettingsArray allTrackSettings() const;
    bool anyTrackSoloed() const;

    // ---- track management (message thread)
    juce::String trackName (int i) const;                 // never empty
    void setTrackName (int i, const juce::String& name);
    juce::String trackNoteName (int i) const;             // "C1" for fixed, "C3" root for scale mode
    int  trackBaseNote (int i) const;                     // the note interval 0 plays
    bool isTrackEnabled (int i) const { return trackSettings (i).enabled; }
    int  numEnabledTracks() const;
    int  addTrack();                                      // enables the first free track, returns its index or -1
    void removeTrack (int i);                             // disables it (content is kept)
    void setBoolParam (int i, const char* suffix, bool value);
    void applyPadLayout (PadLayout layout);
    void clearAll();

    // Send one note of the given track right away (works with the transport stopped).
    void audition (int i);

    // ---- clipboard (message thread)
    void copyTrack (int i);
    void pasteTrack (int i);
    bool hasTrackClip() const { return trackClip.has_value(); }
    void copyPattern();
    void pastePattern();
    bool hasPatternClip() const { return patternClip.has_value(); }
    void clearTrack (int i);

    // Renders the current pattern to a temp .mid file (type 1, one track per enabled
    // sequencer track) and returns it. Used for drag-and-drop export.
    juce::File exportMidiFile (int bars);

    // ---- info for the editor
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool>   uiPlaying { false };
    std::atomic<bool>   uiInternalClock { false };
    std::array<std::atomic<uint32_t>, kNumTracks> hitCount {};   // note-ons emitted per track

    // Editor preferences persisted with the state.
    int   uiTheme = 0;        // 0 dark, 1 light
    float uiScale = 1.0f;
    int   uiExportBars = 4;
    int   uiSelectedTrack = 0;

private:
    void runSequencer (double ppqStart, int numSamples, int sampleOffset, bool playing,
                       const GlobalSettings& g, const TrackSettingsArray& ts, juce::MidiBuffer& midi);
    void runAuditions (int numSamples, juce::MidiBuffer& midi);

    TrackClip makeTrackClip (int i) const;
    void applyTrackClip (int i, const TrackClip& clip);

    std::vector<MidiEvent> eventScratch;
    double internalPpq = 0.0;
    double currentSampleRate = 44100.0;

    std::array<juce::String, kNumTracks> trackNames;

    struct AuditionNote { int channel, note, velocity; };
    juce::AbstractFifo auditionFifo { 64 };
    std::array<AuditionNote, 64> auditionSlots {};
    struct HeldAudition { int channel, note, samplesLeft; };
    std::vector<HeldAudition> heldAuditions;

    std::optional<TrackClip> trackClip;
    std::optional<std::array<TrackClip, kNumTracks>> patternClip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYSequencerProcessor)
};

} // namespace dy
