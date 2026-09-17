#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

// Song mode: a row of chain entries [A x4][B x4]... Click the letter to cycle the
// pattern, drag the number to change bars, + / - add or remove the last entry.
// The playing entry is highlighted with a bar progress strip.
class ChainStrip : public juce::Component
{
public:
    explicit ChainStrip (DYSequencerProcessor& p);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverEntry = -1; repaint(); }

    std::function<void (const juce::String&)> onHint;

private:
    juce::Rectangle<int> entriesArea() const;
    juce::Rectangle<int> entryBounds (int i) const;
    int entryAt (juce::Point<int> p) const;

    DYSequencerProcessor& proc;
    juce::TextButton modeButton { "Chain" }, addButton { "+" }, removeButton { juce::String::charToString (0x2212) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modeAtt;

    int hoverEntry = -1;
    int dragEntry = -1, dragStartBars = 0, dragStartY = 0;
    bool dragged = false;

    static constexpr int kEntryW = 62;
};

} // namespace dy
