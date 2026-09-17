#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Theme.h"

namespace dy {

// One track as a row: [#] [name] [note] [shift] [M] [S] | [steps] [pulses] [rot] | step cells | [x]
class TrackRowView : public juce::Component
{
public:
    TrackRowView (DYSequencerProcessor& p, int index);

    int  trackIndex() const { return index; }
    void setSelected (bool b)  { if (selected != b) { selected = b; repaint(); } }
    void setColumns (int n)    { if (columns != n) { columns = n; repaint(); } }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override { applyButtonColours(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (int)> onSelect, onAudition, onRemove;
    std::function<void (const juce::String&)> onHint;

private:
    juce::Rectangle<int> cellArea() const;
    void applyButtonColours();
    void rebuildNoteAttachment();

    DYSequencerProcessor& proc;
    const int index;
    bool selected = false, hover = false;
    int columns = 16;
    int hoverStep = -1, lastPainted = -1;
    bool paintState = true;
    int noteMode = -1;
    uint32_t lastHits = 0;
    double flashUntil = 0.0;

    juce::Label nameLabel;
    ValueBox noteBox, shiftBox, stepsBox, pulsesBox, rotateBox;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" }, removeBtn { juce::String::charToString (0x00d7) };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> noteAtt, shiftAtt, stepsAtt, pulsesAtt, rotateAtt;
    std::unique_ptr<BA> muteAtt, soloAtt;
};

// Column captions + step numbers above the rows.
class OverviewHeader : public juce::Component
{
public:
    void setColumns (int n) { if (columns != n) { columns = n; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    int columns = 16;
};

// All enabled tracks, one row each, scrolling past kMaxVisibleRows, plus "Add track".
class OverviewGrid : public juce::Component
{
public:
    explicit OverviewGrid (DYSequencerProcessor& p);

    // Recreates rows when the set of enabled tracks changed. Returns true if it did.
    bool rebuildIfNeeded();
    void refresh();
    void setSelected (int i);
    int  preferredHeight() const;

    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void (int)> onSelect, onAudition;
    std::function<void (const juce::String&)> onHint;

private:
    int computeColumns() const;

    DYSequencerProcessor& proc;
    OverviewHeader header;
    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<TrackRowView> rows;
    juce::TextButton addButton { "+  Add track" };
    uint32_t enabledMask = 0xffffffffu;
    int selected = 0;
    int columns = 16;
};

} // namespace dy
