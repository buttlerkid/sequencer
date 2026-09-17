#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

class TrackRow : public juce::Component
{
public:
    TrackRow (DYSequencerProcessor& p, int index);

    void setSelected (bool b) { if (selected != b) { selected = b; repaint(); } }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

    std::function<void (int)> onSelect;

private:
    DYSequencerProcessor& proc;
    const int index;
    bool selected = false, hover = false;

    juce::TextButton onButton, muteButton { "M" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt, muteAtt;
};

// The 16 track rows down the left edge.
class TrackList : public juce::Component
{
public:
    explicit TrackList (DYSequencerProcessor& p);

    void setSelected (int i);
    void refresh() { repaint(); }   // playhead indicators
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (int)> onSelect;

private:
    juce::OwnedArray<TrackRow> rows;
};

} // namespace dy
