#include "OverviewGrid.h"
#include "Layout.h"
#include "Engine/Euclid.h"
#include "Engine/Scale.h"
#include "Params/Parameters.h"

namespace dy {

// ---------------------------------------------------------------- TrackRowView
TrackRowView::TrackRowView (DYSequencerProcessor& p, int i) : proc (p), index (i)
{
    auto id = [i] (const char* suffix) { return ParamIDs::track (i, suffix); };

    nameLabel.setEditable (false, true, true);
    nameLabel.setFont (uiFont (12.0f, true));
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setTooltip ("Double-click to rename");
    nameLabel.onTextChange = [this] { proc.setTrackName (index, nameLabel.getText()); nameLabel.setText (proc.trackName (index), juce::dontSendNotification); };
    nameLabel.onEditorShow = [this] { if (auto* ed = nameLabel.getCurrentTextEditor()) ed->setFont (uiFont (12.0f, true)); };
    addAndMakeVisible (nameLabel);

    noteBox.setTooltip ("Fixed mode: the MIDI note this track plays. Scale mode: transpose in scale degrees (shows the resulting root).");
    addAndMakeVisible (noteBox);

    shiftAtt = std::make_unique<SA> (proc.apvts, id (ParamIDs::shift), shiftBox);
    shiftBox.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1); };
    shiftBox.setTooltip ("Track timing shift in ms");
    addAndMakeVisible (shiftBox);

    stepsAtt = std::make_unique<SA> (proc.apvts, id (ParamIDs::steps), stepsBox);
    stepsBox.setTooltip ("Pattern length in steps");
    addAndMakeVisible (stepsBox);

    pulsesAtt = std::make_unique<SA> (proc.apvts, id (ParamIDs::pulses), pulsesBox);
    pulsesBox.textFromValueFunction = [] (double v) { return v < 0.5 ? juce::String::charToString (0x2013) : juce::String (static_cast<int> (v)); };
    pulsesBox.setTooltip ("Euclidean pulses (0 = off)");
    addAndMakeVisible (pulsesBox);

    rotateAtt = std::make_unique<SA> (proc.apvts, id (ParamIDs::rotate), rotateBox);
    rotateBox.setTooltip ("Rotate the Euclidean pattern");
    addAndMakeVisible (rotateBox);

    muteBtn.setClickingTogglesState (true);
    muteBtn.setTooltip ("Mute");
    muteAtt = std::make_unique<BA> (proc.apvts, id (ParamIDs::mute), muteBtn);
    addAndMakeVisible (muteBtn);

    soloBtn.setClickingTogglesState (true);
    soloBtn.setTooltip ("Solo");
    soloAtt = std::make_unique<BA> (proc.apvts, id (ParamIDs::solo), soloBtn);
    addAndMakeVisible (soloBtn);

    removeBtn.setTooltip ("Remove track (its content is kept)");
    removeBtn.onClick = [this] { if (onRemove) onRemove (index); };
    addAndMakeVisible (removeBtn);

    // Any control interaction selects the row.
    for (auto* c : std::initializer_list<juce::Component*> { &noteBox, &shiftBox, &stepsBox, &pulsesBox, &rotateBox, &muteBtn, &soloBtn, &nameLabel })
        c->addMouseListener (this, false);

    applyButtonColours();
    refresh();
}

void TrackRowView::applyButtonColours()
{
    const auto& t = themeOf (*this);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, t.danger);
    muteBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, t.solo);
    soloBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff10261a));
    removeBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
}

void TrackRowView::rebuildNoteAttachment()
{
    const auto s = proc.trackSettings (index);
    if (s.pitchMode == noteMode) return;
    noteMode = s.pitchMode;

    noteAtt.reset();
    noteAtt = std::make_unique<SA> (proc.apvts, ParamIDs::track (index, noteMode == PitchFixed ? ParamIDs::fixedNote : ParamIDs::transpose), noteBox);
    if (noteMode == PitchFixed)
        noteBox.textFromValueFunction = [] (double v) { return juce::MidiMessage::getMidiNoteName (static_cast<int> (v), true, true, 3); };
    else
        noteBox.textFromValueFunction = [this] (double v)
        {
            const auto g = proc.params.readGlobal();
            return juce::MidiMessage::getMidiNoteName (noteForDegree (g.key, g.scale, kBaseNote, static_cast<int> (v)), true, true, 3);
        };
    noteBox.updateText();
}

void TrackRowView::refresh()
{
    rebuildNoteAttachment();
    if (! nameLabel.isBeingEdited())
        nameLabel.setText (proc.trackName (index), juce::dontSendNotification);

    const auto s = proc.trackSettings (index);
    const bool euclid = s.euclidMode != EuclidOff;
    pulsesBox.setEnabled (euclid);
    rotateBox.setEnabled (euclid);
    if (noteMode == PitchScale) noteBox.updateText();   // key / scale may have changed

    const auto hits = proc.hitCount[static_cast<size_t> (index)].load (std::memory_order_relaxed);
    if (hits != lastHits)
    {
        lastHits = hits;
        flashUntil = juce::Time::getMillisecondCounterHiRes() + 110.0;
    }
    repaint();
}

juce::Rectangle<int> TrackRowView::cellArea() const
{
    return getLocalBounds().withTrimmedLeft (cols::controls).withTrimmedRight (cols::remove + 4).reduced (0, 3);
}

void TrackRowView::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    r.removeFromLeft (cols::index);
    nameLabel.setBounds (r.removeFromLeft (cols::name).reduced (2, 0));
    noteBox.setBounds (r.removeFromLeft (cols::note).reduced (2, 1));
    shiftBox.setBounds (r.removeFromLeft (cols::shift).reduced (2, 1));
    muteBtn.setBounds (r.removeFromLeft (cols::mute).reduced (1, 1));
    soloBtn.setBounds (r.removeFromLeft (cols::solo).reduced (1, 1));
    r.removeFromLeft (cols::gap);
    stepsBox.setBounds (r.removeFromLeft (cols::steps).reduced (2, 1));
    pulsesBox.setBounds (r.removeFromLeft (cols::pulses).reduced (2, 1));
    rotateBox.setBounds (r.removeFromLeft (cols::rotate).reduced (2, 1));
    removeBtn.setBounds (getLocalBounds().removeFromRight (cols::remove + 2).reduced (1, 4));
}

void TrackRowView::paint (juce::Graphics& g)
{
    const auto& t   = themeOf (*this);
    const auto  s   = proc.trackSettings (index);
    const auto& tm  = proc.editPattern().tracks[index];
    const int steps = clampT (s.steps, 1, kMaxSteps);
    const uint64_t mask = s.pulses > 0 ? euclidean (steps, s.pulses, s.rotate) : 0;
    const int playhead = proc.sequencer.currentStep (index);
    const bool soloed  = proc.anyTrackSoloed();
    const bool silenced = s.mute || (soloed && ! s.solo);
    const auto area = cellArea();

    if (selected)
    {
        g.setColour (t.panelAlt.withAlpha (0.55f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f, 0.5f), 4.0f);
        g.setColour (t.accent);
        g.fillRoundedRectangle (2.0f, 4.0f, 3.0f, static_cast<float> (getHeight() - 8), 1.5f);
    }
    else if (hover)
    {
        g.setColour (t.panelAlt.withAlpha (0.3f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f, 0.5f), 4.0f);
    }

    // Track number, lit briefly on every hit.
    const bool flashing = juce::Time::getMillisecondCounterHiRes() < flashUntil;
    g.setColour (flashing ? t.accent : t.textDim);
    g.setFont (uiFont (11.0f, flashing));
    g.drawText (juce::String (index + 1), 9, 0, cols::index - 9, getHeight(), juce::Justification::centredLeft);

    // Beat shading and cells
    for (int i = 0; i < columns; i += 8)
    {
        auto a = stepColumn (area, columns, i, 0.0f);
        auto b = stepColumn (area, columns, juce::jmin (columns - 1, i + 3), 0.0f);
        g.setColour (t.beatShade);
        g.fillRect (juce::Rectangle<float> (a.getX(), 0.0f, b.getRight() - a.getX(), static_cast<float> (getHeight())));
    }

    for (int i = 0; i < columns; ++i)
    {
        const auto cell = stepColumn (area, columns, i, columns > 32 ? 1.0f : 2.0f);
        if (i >= steps)
        {
            g.setColour (t.stepDisabled);
            g.fillRoundedRectangle (cell, 2.0f);
            continue;
        }

        const bool manual = tm.isActive (i);
        const bool eu     = euclidHit (mask, i);
        const bool fires  = stepFires (s.euclidMode, manual, eu);

        juce::Colour fill = t.stepOff;
        if (s.euclidMode == EuclidOnly)  fill = eu ? t.stepEuclid : t.stepOff;
        else if (manual && eu)           fill = t.stepBoth;
        else if (manual)                 fill = t.stepOn;
        else if (eu)                     fill = t.stepEuclid;
        if (silenced && fires)           fill = fill.withAlpha (0.35f);

        g.setColour (fill);
        g.fillRoundedRectangle (cell, 2.0f);

        if (s.euclidMode == EuclidOnly && manual && ! eu)
        {
            g.setColour (t.stepOn.withAlpha (0.5f));
            g.drawRoundedRectangle (cell.reduced (1.0f), 2.0f, 1.0f);
        }
        if (i == hoverStep)
        {
            g.setColour (t.text.withAlpha (0.22f));
            g.fillRoundedRectangle (cell, 2.0f);
        }
        if (i == playhead)
        {
            g.setColour (t.playhead);
            g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, fires ? 1.8f : 1.0f);
        }
    }

    if (silenced)
    {
        g.setColour (s.mute ? t.danger : t.textDim);
        g.setFont (uiFont (10.5f, true));
        g.drawText (s.mute ? "MUTED" : "SOLO ELSEWHERE", area.withTrimmedRight (6), juce::Justification::centredRight);
    }
}

void TrackRowView::mouseDown (const juce::MouseEvent& e)
{
    if (onSelect) onSelect (index);
    if (e.eventComponent != this) return;          // a child control took the click

    const auto pos = e.getEventRelativeTo (this).position;
    const auto area = cellArea();
    if (! area.expanded (0, 3).contains (pos.toInt())) return;

    const int steps = clampT (proc.trackSettings (index).steps, 1, kMaxSteps);
    const int step  = stepAtX (area, columns, pos.x);
    if (step >= steps) return;

    paintState = ! proc.editPattern().tracks[index].isActive (step);
    proc.editPattern().tracks[index].setActive (step, paintState);
    lastPainted = step;
    if (paintState && onAudition) onAudition (index);
    repaint();
}

void TrackRowView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != this || lastPainted < 0) return;
    const auto pos = e.getEventRelativeTo (this).position;
    const int steps = clampT (proc.trackSettings (index).steps, 1, kMaxSteps);
    const int step  = stepAtX (cellArea(), columns, pos.x);
    if (step != lastPainted && step < steps)
    {
        proc.editPattern().tracks[index].setActive (step, paintState);
        lastPainted = step;
        repaint();
    }
}

void TrackRowView::mouseMove (const juce::MouseEvent& e)
{
    hover = true;
    const auto pos = e.getEventRelativeTo (this).position;
    const int step = e.eventComponent == this && cellArea().contains (pos.toInt()) ? stepAtX (cellArea(), columns, pos.x) : -1;
    if (step != hoverStep)
    {
        hoverStep = step;
        if (onHint) onHint (step >= 0 ? "Step " + juce::String (step + 1) + "  -  click to toggle, drag to paint" : juce::String());
        repaint();
    }
}

void TrackRowView::mouseExit (const juce::MouseEvent&)
{
    hover = false;
    hoverStep = -1;
    lastPainted = -1;
    if (onHint) onHint ({});
    repaint();
}

// ---------------------------------------------------------------- OverviewHeader
void OverviewHeader::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.textDim);
    g.setFont (uiFont (10.0f, true));

    auto r = getLocalBounds();
    r.removeFromLeft (cols::index);
    g.drawText ("TRACK", r.removeFromLeft (cols::name).reduced (2, 0), juce::Justification::centredLeft);
    g.drawText ("NOTE",  r.removeFromLeft (cols::note), juce::Justification::centred);
    g.drawText ("SHIFT", r.removeFromLeft (cols::shift), juce::Justification::centred);
    r.removeFromLeft (cols::mute + cols::solo + cols::gap);
    g.drawText ("STEPS", r.removeFromLeft (cols::steps), juce::Justification::centred);
    g.drawText ("PULSE", r.removeFromLeft (cols::pulses), juce::Justification::centred);
    g.drawText ("ROT",   r.removeFromLeft (cols::rotate), juce::Justification::centred);

    const auto area = getLocalBounds().withTrimmedLeft (cols::controls).withTrimmedRight (cols::remove + 4);
    g.setFont (uiFont (10.0f));
    for (int i = 0; i < columns; i += 4)
    {
        const auto c = stepColumn (area, columns, i);
        g.drawText (juce::String (i + 1), c.toNearestInt().withHeight (getHeight()), juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------- OverviewGrid
OverviewGrid::OverviewGrid (DYSequencerProcessor& p) : proc (p)
{
    addAndMakeVisible (header);
    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addButton.setTooltip ("Enable the next free track (up to 16)");
    addButton.onClick = [this]
    {
        const int i = proc.addTrack();
        if (i >= 0)
        {
            rebuildIfNeeded();
            if (onSelect) onSelect (i);
        }
    };
    addAndMakeVisible (addButton);
    rebuildIfNeeded();
}

int OverviewGrid::computeColumns() const
{
    int maxSteps = 16;
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto s = proc.trackSettings (i);
        if (s.enabled) maxSteps = juce::jmax (maxSteps, s.steps);
    }
    return ((maxSteps + 15) / 16) * 16;
}

bool OverviewGrid::rebuildIfNeeded()
{
    uint32_t mask = 0;
    for (int i = 0; i < kNumTracks; ++i)
        if (proc.isTrackEnabled (i)) mask |= (1u << i);
    if (mask == enabledMask) return false;
    enabledMask = mask;

    rows.clear();
    for (int i = 0; i < kNumTracks; ++i)
    {
        if (! (mask & (1u << i))) continue;
        auto* row = rows.add (new TrackRowView (proc, i));
        row->onSelect   = [this] (int idx) { if (onSelect) onSelect (idx); };
        row->onAudition = [this] (int idx) { if (onAudition) onAudition (idx); };
        row->onRemove   = [this] (int idx) { proc.removeTrack (idx); rebuildIfNeeded(); if (onSelect) onSelect (-1); };
        row->onHint     = [this] (const juce::String& h) { if (onHint) onHint (h); };
        row->setSelected (i == selected);
        rowsHolder.addAndMakeVisible (row);
    }
    addButton.setVisible (rows.size() < kNumTracks);
    resized();
    return true;
}

int OverviewGrid::preferredHeight() const
{
    const int n = rows.size();
    return kOverviewHeaderH + juce::jmin (n, kMaxVisibleRows) * kRowH + (n < kNumTracks ? kAddRowH + 2 : 0) + 6;
}

void OverviewGrid::setSelected (int i)
{
    selected = i;
    for (auto* r : rows) r->setSelected (r->trackIndex() == i);
}

void OverviewGrid::refresh()
{
    const int c = computeColumns();
    if (c != columns)
    {
        columns = c;
        header.setColumns (c);
        for (auto* r : rows) r->setColumns (c);
    }
    for (auto* r : rows) r->refresh();
}

void OverviewGrid::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    header.setBounds (r.removeFromTop (kOverviewHeaderH));
    if (addButton.isVisible())
    {
        auto add = r.removeFromBottom (kAddRowH);
        addButton.setBounds (add.removeFromLeft (cols::controls - cols::gap).reduced (cols::index, 2));
        r.removeFromBottom (2);
    }
    viewport.setBounds (r);

    const int n = rows.size();
    const int needScroll = n > kMaxVisibleRows;
    const int w = viewport.getWidth() - (needScroll ? viewport.getScrollBarThickness() : 0);
    rowsHolder.setSize (w, n * kRowH);
    for (int i = 0; i < n; ++i)
        rows[i]->setBounds (0, i * kRowH, w, kRowH);
    header.setColumns (columns);
}

void OverviewGrid::paint (juce::Graphics& g)
{
    g.setColour (themeOf (*this).panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    if (rows.isEmpty())
    {
        g.setColour (themeOf (*this).textDim);
        g.setFont (uiFont (13.0f));
        g.drawText ("No tracks - press  + Add track  or click a pad", viewport.getBounds(), juce::Justification::centred);
    }
}

} // namespace dy
