#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/HeaderBar.h"
#include "UI/ChainStrip.h"
#include "UI/OverviewGrid.h"
#include "UI/PadsPanel.h"
#include "UI/EditPanel.h"
#include "UI/GlobalPanel.h"
#include "UI/StatusBar.h"

namespace dy {

// The editor lays everything out at a fixed logical size and scales the whole
// tree with an AffineTransform, so any window size from 50% to 400% works.
//
//   header
//   overview grid (all tracks, grows with the track count, scrolls past 10)
//   pads | edit panel (selected track) | global
//   status bar
class DYSequencerEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit DYSequencerEditor (DYSequencerProcessor&);
    ~DYSequencerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutPanels();
    void selectTrack (int i);
    void applyTheme();
    void patternChanged();

    DYSequencerProcessor& proc;
    DYLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Component content;
    HeaderBar    header;
    ChainStrip   chainStrip;
    OverviewGrid overview;
    PadsPanel    pads;
    EditPanel    edit;
    GlobalPanel  global;
    StatusBar    status;

    int selectedTrack = -1;
    int lastOverviewHeight = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYSequencerEditor)
};

} // namespace dy
