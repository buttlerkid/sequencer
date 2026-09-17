#include "LaneEditor.h"
#include "Layout.h"
#include "Theme.h"
#include "Engine/Euclid.h"

namespace dy {

LaneEditor::LaneEditor (DYSequencerProcessor& p) : proc (p)
{
    for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
    {
        auto* b = tabs.add (new juce::TextButton (laneInfo (static_cast<Lane> (l)).name));
        b->setClickingTogglesState (false);
        b->setRadioGroupId (1001);
        b->onClick = [this, l] { setLane (static_cast<Lane> (l)); };
        addAndMakeVisible (b);
    }
    tabs[0]->setToggleState (true, juce::dontSendNotification);

    resetButton.setTooltip ("Reset this lane to its default for every step");
    resetButton.onClick = [this] { proc.pattern.tracks[track].resetLane (current); repaint(); };
    addAndMakeVisible (resetButton);

    randomButton.setTooltip ("Randomise this lane across the pattern length");
    randomButton.onClick = [this]
    {
        const auto& info = laneInfo (current);
        const int steps = proc.trackSettings (track).steps;
        // Keep random values musically usable: velocities and lengths stay in the upper half,
        // timing and intervals stay modest, probability spans a useful range.
        switch (current)
        {
            case Lane::Velocity:    generator.randomizeLane (proc.pattern.tracks[track], current, steps, 60, 127); break;
            case Lane::Length:      generator.randomizeLane (proc.pattern.tracks[track], current, steps, 20, 120); break;
            case Lane::Timing:      generator.randomizeLane (proc.pattern.tracks[track], current, steps, -16, 16); break;
            case Lane::Probability: generator.randomizeLane (proc.pattern.tracks[track], current, steps, 40, 100); break;
            case Lane::Repeats:     generator.randomizeLane (proc.pattern.tracks[track], current, steps, 1, 3); break;
            case Lane::Interval:    generator.randomizeLane (proc.pattern.tracks[track], current, steps, -7, 7); break;
            default:                generator.randomizeLane (proc.pattern.tracks[track], current, steps, info.min, info.max); break;
        }
        repaint();
    };
    addAndMakeVisible (randomButton);

    readout.setJustificationType (juce::Justification::centredRight);
    readout.setFont (uiFont (12.0f));
    addAndMakeVisible (readout);
}

void LaneEditor::setLane (Lane l)
{
    current = l;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == static_cast<int> (l), juce::dontSendNotification);
    repaint();
}

void LaneEditor::resized()
{
    auto tabsRow = tabArea();
    const int w = 92;
    for (auto* b : tabs)
        b->setBounds (tabsRow.removeFromLeft (w).reduced (2, 1));

    randomButton.setBounds (tabsRow.removeFromRight (76).reduced (2, 1));
    resetButton.setBounds (tabsRow.removeFromRight (64).reduced (2, 1));
    readout.setBounds (tabsRow.reduced (4, 0));
}

int LaneEditor::valueAtY (float y) const
{
    const auto& info = laneInfo (current);
    const auto area = barArea();
    const float norm = 1.0f - (y - area.getY()) / static_cast<float> (juce::jmax (1, area.getHeight()));
    return clampT (static_cast<int> (std::lround (info.min + norm * (info.max - info.min))), info.min, info.max);
}

juce::String LaneEditor::describe (int step) const
{
    if (step < 0) return {};
    const auto& info = laneInfo (current);
    const int v = proc.pattern.tracks[track].get (current, step);
    juce::String s = "Step " + juce::String (step + 1) + "   " + juce::String (info.name) + " ";
    if (info.bipolar && v > 0) s += "+";
    s += juce::String (v) + juce::String (info.unit);
    return s;
}

void LaneEditor::paint (juce::Graphics& g)
{
    const auto& t    = themeOf (*this);
    const auto  s    = proc.trackSettings (track);
    const auto& tm   = proc.pattern.tracks[track];
    const auto& info = laneInfo (current);
    const int steps  = clampT (s.steps, 1, kMaxSteps);
    const uint64_t mask = s.pulses > 0 ? euclidean (steps, s.pulses, s.rotate) : 0;
    const int playhead = proc.sequencer.currentStep (track);
    const auto area = barArea();

    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().withTrimmedTop (28).toFloat(), 6.0f);

    for (int i = 0; i < steps; i += 8)
    {
        auto a = stepColumn (area, steps, i, 0.0f);
        auto b = stepColumn (area, steps, juce::jmin (steps - 1, i + 3), 0.0f);
        g.setColour (t.beatShade);
        g.fillRect (juce::Rectangle<float> (a.getX(), static_cast<float> (area.getY() - 6), b.getRight() - a.getX(), static_cast<float> (area.getHeight() + 12)));
    }

    const float range = static_cast<float> (info.max - info.min);
    const float zeroY = info.bipolar ? area.getBottom() - ((0 - info.min) / range) * area.getHeight()
                                     : static_cast<float> (area.getBottom());

    if (info.bipolar)
    {
        g.setColour (t.centreLine);
        g.drawHorizontalLine (static_cast<int> (zeroY), static_cast<float> (area.getX()), static_cast<float> (area.getRight()));
    }

    for (int i = 0; i < steps; ++i)
    {
        const auto col = stepColumn (area, steps, i);
        const int v = tm.get (current, i);
        const float vy = area.getBottom() - ((v - info.min) / range) * area.getHeight();

        const bool manual = tm.isActive (i);
        const bool eu     = euclidHit (mask, i);
        const bool fires  = s.euclidMode == EuclidOff ? manual : s.euclidMode == EuclidAdd ? (manual || eu) : eu;

        // Ghost bar shows the full column so empty steps are still targets.
        g.setColour (t.stepOff.withAlpha (0.5f));
        g.fillRoundedRectangle (col, 2.0f);

        juce::Rectangle<float> bar;
        if (info.bipolar)
            bar = juce::Rectangle<float> (col.getX(), juce::jmin (vy, zeroY), col.getWidth(), juce::jmax (2.0f, std::abs (zeroY - vy)));
        else
            bar = juce::Rectangle<float> (col.getX(), vy, col.getWidth(), juce::jmax (2.0f, area.getBottom() - vy));

        g.setColour (fires ? t.bar : t.barDim);
        g.fillRoundedRectangle (bar, 2.0f);

        if (i == hoverStep)
        {
            g.setColour (t.text.withAlpha (0.18f));
            g.fillRoundedRectangle (col, 2.0f);
        }
        if (i == playhead)
        {
            g.setColour (t.playhead.withAlpha (0.6f));
            g.drawRoundedRectangle (col.reduced (0.5f), 2.0f, 1.0f);
        }
    }

    // Value ticks on the right
    g.setColour (t.textDim);
    g.setFont (uiFont (10.0f));
    g.drawText (juce::String (info.max) + info.unit, area.getRight() - 46, area.getY() - 2, 44, 12, juce::Justification::right);
    g.drawText (juce::String (info.min) + info.unit, area.getRight() - 46, area.getBottom() - 10, 44, 12, juce::Justification::right);
}

void LaneEditor::applyAt (const juce::MouseEvent& e)
{
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = stepAtX (barArea(), steps, e.position.x);
    const int value = valueAtY (e.position.y);

    // Horizontal sweeps fill every column crossed since the last event.
    if (lastStep >= 0 && std::abs (step - lastStep) > 1)
    {
        const int dir = step > lastStep ? 1 : -1;
        for (int i = lastStep + dir; i != step; i += dir)
            proc.pattern.tracks[track].set (current, i, value);
    }
    proc.pattern.tracks[track].set (current, step, value);
    lastStep = step;
    readout.setText (describe (step), juce::dontSendNotification);
    repaint();
}

void LaneEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! barArea().expanded (0, 6).contains (e.position.toInt())) return;
    lastStep = -1;
    applyAt (e);
}

void LaneEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (lastStep < 0) return;
    applyAt (e);
}

void LaneEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! barArea().expanded (0, 6).contains (e.position.toInt())) return;
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = stepAtX (barArea(), steps, e.position.x);
    proc.pattern.tracks[track].set (current, step, laneInfo (current).def);
    readout.setText (describe (step), juce::dontSendNotification);
    repaint();
}

void LaneEditor::mouseMove (const juce::MouseEvent& e)
{
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = barArea().expanded (0, 6).contains (e.position.toInt()) ? stepAtX (barArea(), steps, e.position.x) : -1;
    if (step != hoverStep)
    {
        hoverStep = step;
        readout.setText (describe (step), juce::dontSendNotification);
        if (onHint) onHint (step >= 0 ? "Drag to set " + juce::String (laneInfo (current).name).toLowerCase() + ", double-click to reset" : juce::String());
        repaint();
    }
}

void LaneEditor::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1;
    readout.setText ({}, juce::dontSendNotification);
    if (onHint) onHint ({});
    repaint();
}

} // namespace dy
