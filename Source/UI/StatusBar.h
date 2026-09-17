#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

class StatusBar : public juce::Component
{
public:
    explicit StatusBar (DYSequencerProcessor& p) : proc (p) {}

    void setHint (const juce::String& h) { if (hint != h) { hint = h; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    DYSequencerProcessor& proc;
    juce::String hint;
};

} // namespace dy
