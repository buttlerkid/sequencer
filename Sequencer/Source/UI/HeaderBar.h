#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace dy {

// Drag this onto a DAW track to drop a .mid of the pattern; click it to save one.
class MidiDragSource : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    MidiDragSource (DYSequencerProcessor& p, std::function<int()> barsProvider);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    DYSequencerProcessor& proc;
    std::function<int()> bars;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragging = false, hover = false;
};

// One of the A..H pattern slots: filled when it is the edited (target) pattern,
// ringed while it is still the playing one, blinking while a change is pending.
class PatternButton : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PatternButton (DYSequencerProcessor& p, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
private:
    DYSequencerProcessor& proc;
    const int index;
    bool hover = false;
};

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (DYSequencerProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void()> onAddTrack;
    std::function<void()> onPatternChanged;
    std::function<void()> onThemeToggle;

private:
    DYSequencerProcessor& proc;

    juce::TextButton addTrack { "+  Add track" }, clearAll { "Clear all" }, copyAll { "Copy All" }, pasteAll { "Paste All" };
    juce::OwnedArray<PatternButton> patternButtons;
    juce::TextButton fillButton { "FILL" }, themeButton;
    juce::Label     barsLabel;
    juce::ComboBox  exportBars;
    MidiDragSource  dragSource;
};

} // namespace dy
