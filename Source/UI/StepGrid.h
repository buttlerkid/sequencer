#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

// One row of step cells for the selected track. Click toggles, drag paints.
// Cells show manual steps, Euclid-generated steps, and the playhead.
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (DYSequencerProcessor& p) : proc (p) {}

    void setTrack (int i) { track = i; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (const juce::String&)> onHint;

private:
    juce::Rectangle<int> cellArea() const { return getLocalBounds().reduced (0, 14).withTrimmedTop (2); }
    void paintStep (int step);

    DYSequencerProcessor& proc;
    int track = 0;
    bool paintState = true;
    int lastPainted = -1;
    int hoverStep = -1;
};

} // namespace dy
