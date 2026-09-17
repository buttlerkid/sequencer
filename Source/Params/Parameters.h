#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Types.h"

namespace dy {

// Host-automatable parameters. Per-step data is NOT here (see PatternModel); it
// lives in the plugin state as plain values so the parameter count stays sane.
namespace ParamIDs
{
    inline const juce::String key          = "key";
    inline const juce::String scale        = "scale";
    inline const juce::String swingProfile = "swingProfile";
    inline const juce::String swingAmount  = "swingAmount";
    inline const juce::String masterShift  = "masterShift";

    // Per-track suffixes; full id is track (i, suffix)
    inline const char* const enabled      = "enabled";
    inline const char* const mute         = "mute";
    inline const char* const channel      = "channel";
    inline const char* const steps        = "steps";
    inline const char* const division     = "division";
    inline const char* const pulses       = "pulses";
    inline const char* const rotate       = "rotate";
    inline const char* const euclidMode   = "euclidMode";
    inline const char* const pitchMode    = "pitchMode";
    inline const char* const fixedNote    = "fixedNote";
    inline const char* const transpose    = "transpose";
    inline const char* const swingMode    = "swingMode";
    inline const char* const swingProfileT = "swingProfile";
    inline const char* const swingAmountT  = "swingAmount";
    inline const char* const solo         = "solo";
    inline const char* const shift        = "shift";
    inline const char* const velOffset    = "velOffset";
    inline const char* const lengthScale  = "lengthScale";
    inline const char* const probScale    = "probScale";
    inline const char* const repsAdd      = "repsAdd";

    inline juce::String track (int index, const char* suffix)
    {
        return "t" + juce::String (index + 1) + "_" + suffix;
    }
}

extern const juce::StringArray kDivisionNames;
extern const double            kDivisionPpq[];
extern const int               kNumDivisions;

juce::StringArray keyNames();
juce::StringArray scaleNames();
juce::StringArray shuffleProfileNames();
juce::StringArray euclidModeNames();
juce::StringArray pitchModeNames();
juce::StringArray swingModeNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Cached raw-value pointers so the audio thread never does string lookups.
struct TrackParamRefs
{
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* mute = nullptr;
    std::atomic<float>* channel = nullptr;
    std::atomic<float>* steps = nullptr;
    std::atomic<float>* division = nullptr;
    std::atomic<float>* pulses = nullptr;
    std::atomic<float>* rotate = nullptr;
    std::atomic<float>* euclidMode = nullptr;
    std::atomic<float>* pitchMode = nullptr;
    std::atomic<float>* fixedNote = nullptr;
    std::atomic<float>* transpose = nullptr;
    std::atomic<float>* swingMode = nullptr;
    std::atomic<float>* swingProfile = nullptr;
    std::atomic<float>* swingAmount = nullptr;
    std::atomic<float>* solo = nullptr;
    std::atomic<float>* shift = nullptr;
    std::atomic<float>* velOffset = nullptr;
    std::atomic<float>* lengthScale = nullptr;
    std::atomic<float>* probScale = nullptr;
    std::atomic<float>* repsAdd = nullptr;

    TrackSettings read() const;
};

struct ParamRefs
{
    std::atomic<float>* key = nullptr;
    std::atomic<float>* scale = nullptr;
    std::atomic<float>* swingProfile = nullptr;
    std::atomic<float>* swingAmount = nullptr;
    std::atomic<float>* masterShift = nullptr;
    std::array<TrackParamRefs, kNumTracks> tracks;

    void bind (juce::AudioProcessorValueTreeState& apvts);
    GlobalSettings readGlobal() const;
};

// All per-track parameter ids, in a stable order (used by copy / paste).
const std::vector<const char*>& trackParamSuffixes();

// Routing parameters stay with the destination track on paste.
bool isRoutingParam (const char* suffix);

} // namespace dy
