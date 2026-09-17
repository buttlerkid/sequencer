#include "PluginEditor.h"
#include "UI/Layout.h"

namespace dy {

DYSequencerEditor::DYSequencerEditor (DYSequencerProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      lookAndFeel (p.uiTheme == 0 ? Theme::dark() : Theme::light()),
      header (p), overview (p), pads (p), edit (p), global (p), status (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &header, &overview, &pads, &edit, &global, &status })
        content.addAndMakeVisible (c);

    auto hint = [this] (const juce::String& h) { status.setHint (h); };
    overview.onHint = hint;
    edit.onHint     = hint;

    header.onAddTrack = [this]
    {
        const int i = proc.addTrack();
        overview.rebuildIfNeeded();
        if (i >= 0) selectTrack (i);
    };
    header.onPatternChanged = [this] { patternChanged(); };
    edit.onPatternChanged   = [this] { patternChanged(); };

    overview.onSelect   = [this] (int i) { selectTrack (i); };
    overview.onAudition = [this] (int i) { proc.audition (i); };

    pads.onPadClick = [this] (int i)
    {
        if (! proc.isTrackEnabled (i))
        {
            proc.setBoolParam (i, ParamIDs::enabled, true);
            overview.rebuildIfNeeded();
        }
        selectTrack (i);
        proc.audition (i);
    };
    pads.onLayoutApplied = [this] { patternChanged(); };

    header.onThemeToggle = [this]
    {
        proc.uiTheme = proc.uiTheme == 0 ? 1 : 0;
        applyTheme();
    };

    selectTrack (proc.isTrackEnabled (proc.uiSelectedTrack) ? proc.uiSelectedTrack : -1);

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
    header.refresh();
    edit.refresh();
    overview.refresh();
    pads.repaint();
}

void DYSequencerEditor::selectTrack (int i)
{
    if (i >= 0 && ! proc.isTrackEnabled (i))
        i = -1;
    if (i < 0)
        for (int k = 0; k < kNumTracks && i < 0; ++k)
            if (proc.isTrackEnabled (k)) i = k;

    selectedTrack = i;
    proc.uiSelectedTrack = juce::jmax (0, i);
    overview.setSelected (i);
    pads.setSelected (i);
    edit.setTrack (i);
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
    layoutPanels();
}

void DYSequencerEditor::layoutPanels()
{
    auto r = content.getLocalBounds();
    header.setBounds (r.removeFromTop (kHeaderH));
    status.setBounds (r.removeFromBottom (kStatusH));
    r = r.reduced (8, 0).withTrimmedBottom (4);

    lastOverviewHeight = overview.preferredHeight();
    overview.setBounds (r.removeFromTop (lastOverviewHeight));
    r.removeFromTop (8);

    auto left = r.removeFromLeft (250);
    pads.setBounds (left.removeFromTop (juce::jmin (270, left.getHeight() * 46 / 100)));
    left.removeFromTop (8);
    global.setBounds (left);
    r.removeFromLeft (8);
    edit.setBounds (r);
}

void DYSequencerEditor::timerCallback()
{
    // Tracks added / removed from the host side, and a selection that vanished.
    if (overview.rebuildIfNeeded() || overview.preferredHeight() != lastOverviewHeight)
    {
        layoutPanels();
        header.refresh();
        if (selectedTrack < 0 || ! proc.isTrackEnabled (selectedTrack))
            selectTrack (-1);
    }

    overview.refresh();
    pads.refresh();
    edit.refresh();
    header.refresh();
    global.refresh();
    status.repaint();
}

} // namespace dy
