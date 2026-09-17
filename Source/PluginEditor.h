#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/HeaderBar.h"
#include "UI/TrackList.h"
#include "UI/TrackPanel.h"
#include "UI/StepGrid.h"
#include "UI/LaneEditor.h"
#include "UI/StatusBar.h"

namespace dy {

// The editor lays everything out at a fixed logical size and scales the whole
// tree with an AffineTransform, so any window size from 50% to 400% works.
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
    void selectTrack (int i);
    void applyTheme();
    void patternChanged();

    DYSequencerProcessor& proc;
    DYLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Component content;
    HeaderBar   header;
    TrackList   trackList;
    TrackPanel  trackPanel;
    StepGrid    grid;
    LaneEditor  lanes;
    StatusBar   status;

    int selectedTrack = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DYSequencerEditor)
};

} // namespace dy
