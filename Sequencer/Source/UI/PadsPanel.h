#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

// 4x4 pads, one per track: shows the note and name, lights up on hits,
// click to select (enabling the track if needed) and audition.
class PadsPanel : public juce::Component
{
public:
    explicit PadsPanel (DYSequencerProcessor& p);

    void setSelected (int i) { selected = i; repaint(); }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (int)> onPadClick;          // select + audition (+ enable)
    std::function<void()> onLayoutApplied;

private:
    juce::Rectangle<int> padArea() const { return getLocalBounds().withTrimmedTop (30).reduced (6); }
    juce::Rectangle<float> padBounds (int i) const;
    int padAt (juce::Point<float> p) const;

    DYSequencerProcessor& proc;
    juce::TextButton layoutButton { "Layout" };
    int selected = 0, hoverPad = -1;
    std::array<uint32_t, kNumTracks> lastHits {};
    std::array<double, kNumTracks> flashUntil {};
};

} // namespace dy
