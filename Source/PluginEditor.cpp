#include "PluginEditor.h"
#include "UI/Layout.h"

namespace dy {

DYSequencerEditor::DYSequencerEditor (DYSequencerProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      lookAndFeel (p.uiTheme == 0 ? Theme::dark() : Theme::light()),
      header (p), trackList (p), trackPanel (p), grid (p), lanes (p), status (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (trackList);
    content.addAndMakeVisible (trackPanel);
    content.addAndMakeVisible (grid);
    content.addAndMakeVisible (lanes);
    content.addAndMakeVisible (status);

    trackList.onSelect = [this] (int i) { selectTrack (i); };
    header.onThemeToggle = [this]
    {
        proc.uiTheme = proc.uiTheme == 0 ? 1 : 0;
        applyTheme();
    };
    header.onPatternChanged     = [this] { patternChanged(); };
    trackPanel.onPatternChanged = [this] { patternChanged(); };
    grid.onHint  = [this] (const juce::String& h) { status.setHint (h); };
    lanes.onHint = [this] (const juce::String& h) { status.setHint (h); };
    trackPanel.onHint = grid.onHint;

    selectTrack (proc.uiSelectedTrack);

    // Read the saved scale before anything can trigger resized().
    const float scale = juce::jlimit (0.5f, 4.0f, proc.uiScale);

    setResizable (true, true);
    setResizeLimits (kLogicalW / 2, kLogicalH / 2, kLogicalW * 4, kLogicalH * 4);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kLogicalW) / kLogicalH);
    setSize (juce::roundToInt (kLogicalW * scale), juce::roundToInt (kLogicalH * scale));

    startTimerHz (30);
}

DYSequencerEditor::~DYSequencerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DYSequencerEditor::applyTheme()
{
    lookAndFeel.setTheme (proc.uiTheme == 0 ? Theme::dark() : Theme::light());
    header.refresh();
    sendLookAndFeelChange();
    repaint();
}

void DYSequencerEditor::patternChanged()
{
    trackPanel.refresh();
    header.refresh();
    grid.repaint();
    lanes.repaint();
}

void DYSequencerEditor::selectTrack (int i)
{
    selectedTrack = juce::jlimit (0, kNumTracks - 1, i);
    proc.uiSelectedTrack = selectedTrack;
    trackList.setSelected (selectedTrack);
    trackPanel.setTrack (selectedTrack);
    grid.setTrack (selectedTrack);
    lanes.setTrack (selectedTrack);
}

void DYSequencerEditor::paint (juce::Graphics& g)
{
    g.fillAll (lookAndFeel.theme().bg);
}

void DYSequencerEditor::resized()
{
    if (getWidth() <= 0) return;
    const float scale = getWidth() / static_cast<float> (kLogicalW);
    proc.uiScale = scale;

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kLogicalW, kLogicalH);

    auto r = content.getLocalBounds();
    header.setBounds (r.removeFromTop (kHeaderH));
    status.setBounds (r.removeFromBottom (kStatusH));
    r = r.reduced (8, 0).withTrimmedBottom (4);

    trackList.setBounds (r.removeFromLeft (kTrackListW));
    r.removeFromLeft (8);

    trackPanel.setBounds (r.removeFromTop (150));
    r.removeFromTop (8);
    grid.setBounds (r.removeFromTop (74));
    r.removeFromTop (8);
    lanes.setBounds (r);
}

void DYSequencerEditor::timerCallback()
{
    // Playheads and host-driven parameter changes.
    trackList.refresh();
    grid.repaint();
    lanes.repaint();
    status.repaint();
    trackPanel.refresh();
}

} // namespace dy
