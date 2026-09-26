#pragma once
#include "Controls.h"
#include "NodalLookAndFeel.h"

namespace dy::nodal {

// The "Play" tab: how the body is played (effect, key follow, instrument), the
// exciter for MIDI notes, voice settings, the sidechain, and the voices ringing now.
class PlayPanel : public juce::Component
{
public:
    explicit PlayPanel (NodalProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();

private:
    void updateHelp();

    NodalProcessor& proc;
    ChoiceButtons playMode, exciter, sidechain;
    Knob tone, attack, release, damp, velocity, voices;
    juce::Label playHelp, exciterHelp;
    juce::Rectangle<int> playSlot, excSlot, voiceSlot, leds;
    int shownMode = -1, shownExc = -1;
    bool shownSc = false;
    std::array<int, kMaxVoices> ledNote {};
    std::array<float, kMaxVoices> ledLevel {};
    std::array<bool, kMaxVoices> ledHeld {};
};

} // namespace dy::nodal
