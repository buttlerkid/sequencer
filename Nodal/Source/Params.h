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
    // v0.2 modulation
    inline const juce::String lfoRate[2]   = { "lfo1Rate", "lfo2Rate" };
    inline const juce::String lfoSync[2]   = { "lfo1Sync", "lfo2Sync" };
    inline const juce::String lfoDiv[2]    = { "lfo1Div", "lfo2Div" };
    inline const juce::String lfoShape[2]  = { "lfo1Shape", "lfo2Shape" };
    inline const juce::String lfoTarget[2] = { "lfo1Target", "lfo2Target" };
    inline const juce::String lfoAmount[2] = { "lfo1Amount", "lfo2Amount" };
    inline const juce::String envAttack = "envAttack", envRelease = "envRelease", envTarget = "envTarget", envAmount = "envAmount",
        trackPitch = "trackPitch";
}

juce::StringArray bodyNames();
juce::StringArray materialNames();
juce::StringArray scaleNames();
juce::StringArray keyNames();
juce::StringArray tuneModeNames();
juce::StringArray modTargetNames();
juce::StringArray lfoShapeNames();
juce::StringArray syncDivNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Cached raw parameter pointers, read on the audio thread without string lookups.
struct ParamRefs
{
    std::atomic<float>* body = nullptr, *material = nullptr, *pitch = nullptr, *key = nullptr, *scale = nullptr,
                      *snap = nullptr, *glide = nullptr, *tuneMode = nullptr, *lock = nullptr, *density = nullptr,
                      *decay = nullptr, *damping = nullptr, *brightness = nullptr, *strikeX = nullptr, *strikeY = nullptr,
                      *spread = nullptr, *lowCut = nullptr, *highCut = nullptr, *drive = nullptr, *mix = nullptr, *output = nullptr;
    std::atomic<float>* lfoRate[2] {}, *lfoSync[2] {}, *lfoDiv[2] {}, *lfoShape[2] {}, *lfoTarget[2] {}, *lfoAmount[2] {};
    std::atomic<float>* envAttack = nullptr, *envRelease = nullptr, *envTarget = nullptr, *envAmount = nullptr, *trackPitch = nullptr;

    void bind (juce::AudioProcessorValueTreeState& s);
    EngineParams read() const;
};

} // namespace dy::nodal
