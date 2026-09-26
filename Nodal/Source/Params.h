#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/NodalEngine.h"

namespace dy::nodal {

namespace PID
{
    inline const juce::String body = "body", material = "material", pitch = "pitch", key = "key", scale = "scale",
        snap = "snap", glide = "glide", tuneMode = "tuneMode", lock = "lock", density = "density", decay = "decay",
        damping = "damping", brightness = "brightness", strikeX = "strikeX", strikeY = "strikeY", spread = "spread",
        lowCut = "lowCut", highCut = "highCut", drive = "drive", mix = "mix", output = "output";
}

juce::StringArray bodyNames();
juce::StringArray materialNames();
juce::StringArray scaleNames();
juce::StringArray keyNames();
juce::StringArray tuneModeNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Cached raw parameter pointers, read on the audio thread without string lookups.
struct ParamRefs
{
    std::atomic<float>* body = nullptr, *material = nullptr, *pitch = nullptr, *key = nullptr, *scale = nullptr,
                      *snap = nullptr, *glide = nullptr, *tuneMode = nullptr, *lock = nullptr, *density = nullptr,
                      *decay = nullptr, *damping = nullptr, *brightness = nullptr, *strikeX = nullptr, *strikeY = nullptr,
                      *spread = nullptr, *lowCut = nullptr, *highCut = nullptr, *drive = nullptr, *mix = nullptr, *output = nullptr;

    void bind (juce::AudioProcessorValueTreeState& s);
    EngineParams read() const;
};

} // namespace dy::nodal
