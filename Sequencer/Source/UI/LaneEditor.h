#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Engine/Generator.h"

namespace dy {

// Lane tabs + a bar editor for the selected lane of the selected track.
// Drag vertically to set a value, horizontally to paint across steps,
// double-click a step to reset it.
class LaneEditor : public juce::Component
{
public:
    explicit LaneEditor (DYSequencerProcessor& p);

    void setTrack (int i) { track = i; repaint(); }
    void setLane (Lane l);
    Lane lane() const { return current; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (const juce::String&)> onHint;

private:
    juce::Rectangle<int> tabArea() const  { return getLocalBounds().removeFromTop (26); }
    juce::Rectangle<int> barArea() const  { return getLocalBounds().withTrimmedTop (30).reduced (0, 6); }
    int  valueAtY (float y) const;
    void applyAt (const juce::MouseEvent& e);
    juce::String describe (int step) const;

    DYSequencerProcessor& proc;
    Generator generator;
    int track = 0;
    Lane current = Lane::Velocity;
    int hoverStep = -1;
    int lastStep = -1;

    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton resetButton { "Reset" }, randomButton { "Random" };
    juce::Label readout;
};

} // namespace dy
