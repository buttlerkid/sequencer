#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Pattern.h"
#include "Engine/Sequencer.h"
#include "Params/Parameters.h"
#include <map>
#include <optional>

namespace dy {

// Clipboard payload for one track: step data + settings, its non-routing
// automatable parameters (normalised, in trackParamSuffixes() order) and its name.
struct TrackClip
{
    TrackSnapshot steps;
    std::vector<float> params;
    juce::String name;
};

enum class PadLayout : int { GmDrums = 0, ChromaticC1, Melodic };

struct ChainEntry { int pattern = 0; int bars = 4; };

class DYSequencerProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::Timer
{
public:
    DYSequencerProcessor();
    ~DYSequencerProcessor() override;

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
    std::array<PatternModel, kNumPatterns> patterns;
    Sequencer    sequencer;

    // The pattern the editor works on: the one selected by the Pattern parameter.
    // In pattern mode it becomes the playing pattern at the next bar line; in chain
    // mode the chain decides what plays and this is just what you edit.
    int           targetPattern() const;
    int           playingPattern() const { return currentPattern.load (std::memory_order_relaxed); }
    bool          patternChangePending() const { return ! chainEnabled() && targetPattern() != playingPattern(); }
    PatternModel& editPattern()             { return patterns[static_cast<size_t> (targetPattern())]; }
    const PatternModel& editPattern() const { return patterns[static_cast<size_t> (targetPattern())]; }
    void          selectPattern (int i);

    // ---- chain (song mode); entries are edited on the message thread
    bool       chainEnabled() const { return params.chainMode->load() > 0.5f && chainLength.load() > 0; }
    int        chainSize() const { return chainLength.load (std::memory_order_relaxed); }
    ChainEntry chainEntry (int i) const;
    void       setChainEntry (int i, ChainEntry e);
    void       addChainEntry();
    void       removeLastChainEntry();
    int        chainTotalBars() const;
    // Which entry / bar within it the transport is on (from the last block's position)
    void       chainPosition (int& entry, int& barInEntry) const;

    TrackSettings      trackSettings (int i) const { return params.tracks[static_cast<size_t> (i)].read(); }
    TrackSettingsArray allTrackSettings() const;
    bool anyTrackSoloed() const;

    // ---- track management (message thread)
    juce::String trackName (int i) const;
    void setTrackName (int i, const juce::String& name);
    juce::String trackNoteName (int i) const;
    int  trackBaseNote (int i) const;
    bool isTrackEnabled (int i) const { return trackSettings (i).enabled; }
    int  numEnabledTracks() const;
    int  addTrack();
    void removeTrack (int i);
    void setBoolParam (int i, const char* suffix, bool value);
    void applyPadLayout (PadLayout layout);
    void clearAll();

    void audition (int i);

    // ---- clipboard (message thread)
    void copyTrack (int i);
    void pasteTrack (int i);
    bool hasTrackClip() const { return trackClip.has_value(); }
    void copyPattern();
    void pastePattern();
    bool hasPatternClip() const { return patternClip.has_value(); }
    void clearTrack (int i);

    // Renders the edited pattern (or the whole chain in chain mode) to a temp .mid.
    juce::File exportMidiFile (int bars);

    // ---- info for the editor
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<double> uiPpq { 0.0 };
    std::atomic<bool>   uiPlaying { false };
    std::atomic<bool>   uiInternalClock { false };
    std::atomic<int>    midiTranspose { 0 };
    std::array<std::atomic<int>, 8> uiChord {};
    std::atomic<int>    uiChordSize { 0 };
    std::array<std::atomic<uint32_t>, kNumTracks> hitCount {};

    int   uiTheme = 0;
    float uiScale = 1.0f;
    int   uiExportBars = 4;
    int   uiSelectedTrack = 0;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;
    void pushPatternSettingsToParams();
    void copyParamsToAllPatternSettings();

    void runSequencer (double ppqStart, int numSamples, int sampleOffset, bool playing,
                       const GlobalSettings& g, const TrackSettingsArray& ts, const PatternSchedule& sched,
                       juce::MidiBuffer& midi);
    void runAuditions (int numSamples, juce::MidiBuffer& midi);
    void readIncomingMidi (juce::MidiBuffer& midi, int mode);
    int  expandChain();                                     // fills barMap, returns total bars
    static double nextBarAtOrAfter (double ppq) { return std::ceil (ppq / 4.0 - 1e-9) * 4.0; }

    TrackClip makeTrackClip (int i) const;
    void applyTrackClip (int i, const TrackClip& clip);

    std::vector<MidiEvent> eventScratch;
    double internalPpq = 0.0;
    double currentSampleRate = 44100.0;
    std::atomic<int> currentPattern { 0 };

    std::array<std::atomic<int>, kMaxChainEntries> chainPatterns {};
    std::array<std::atomic<int>, kMaxChainEntries> chainBars {};
    std::atomic<int> chainLength { 1 };
    std::array<const PatternModel*, kMaxChainBars> barMap {};

    struct ParamRef { int track; int field; };            // field: 0 steps, 1 pulses, 2 rotate, 3 euclid
    std::map<juce::String, ParamRef> settingParamIds;
    std::atomic<bool> settingsDirty { true };
    bool syncingParams = false;

    std::array<juce::String, kNumTracks> trackNames;

    struct AuditionNote { int channel, note, velocity; };
    juce::AbstractFifo auditionFifo { 64 };
    std::array<AuditionNote, 64> auditionSlots {};
    struct HeldAudition { int channel, note, samplesLeft; };
    std::vector<HeldAudition> heldAuditions;

    std::vector<int> heldNotes;                              // chord follow, audio thread
    int chord[8] = {};
    int chordSize = 0;

    std::optional<TrackClip> trackClip;
    std::optional<std::array<TrackClip, kNumTracks>> patternClip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYSequencerProcessor)
};

} // namespace dy
