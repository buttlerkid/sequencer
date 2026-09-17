#include "StepGrid.h"
#include "Layout.h"
#include "Theme.h"
#include "Engine/Euclid.h"

namespace dy {

void StepGrid::paint (juce::Graphics& g)
{
    const auto& t   = themeOf (*this);
    const auto  s   = proc.trackSettings (track);
    const auto& tm  = proc.pattern.tracks[track];
    const int steps = clampT (s.steps, 1, kMaxSteps);
    const uint64_t mask = s.pulses > 0 ? euclidean (steps, s.pulses, s.rotate) : 0;
    const int playhead = proc.sequencer.currentStep (track);
    const auto area = cellArea();

    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Beat shading: alternate groups of four.
    for (int i = 0; i < steps; i += 8)
    {
        auto a = stepColumn (area, steps, i, 0.0f);
        auto b = stepColumn (area, steps, juce::jmin (steps - 1, i + 3), 0.0f);
        g.setColour (t.beatShade);
        g.fillRect (juce::Rectangle<float> (a.getX(), 0.0f, b.getRight() - a.getX(), static_cast<float> (getHeight())));
    }

    g.setFont (uiFont (10.0f));
    for (int i = 0; i < steps; ++i)
    {
        const auto cell = stepColumn (area, steps, i);
        const bool manual = tm.isActive (i);
        const bool eu     = euclidHit (mask, i);
        const bool fires  = s.euclidMode == EuclidOff ? manual : s.euclidMode == EuclidAdd ? (manual || eu) : eu;

        juce::Colour fill = t.stepOff;
        if (s.euclidMode == EuclidOnly)
            fill = eu ? t.stepEuclid : (manual ? t.stepOff.brighter (0.15f) : t.stepOff);
        else if (manual && eu) fill = t.stepBoth;
        else if (manual)       fill = t.stepOn;
        else if (eu)           fill = t.stepEuclid;

        if (! s.enabled || s.mute)
            fill = fill.withAlpha (0.45f);

        g.setColour (fill);
        g.fillRoundedRectangle (cell, 3.0f);

        if (s.euclidMode == EuclidOnly && manual && ! eu)
        {
            g.setColour (t.stepOn.withAlpha (0.6f));
            g.drawRoundedRectangle (cell.reduced (1.0f), 3.0f, 1.0f);
        }

        if (i == hoverStep)
        {
            g.setColour (t.text.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 3.0f);
        }

        if (i == playhead)
        {
            g.setColour (t.playhead);
            g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, fires ? 2.0f : 1.0f);
        }

        if (i % 4 == 0)
        {
            g.setColour (t.textDim);
            g.drawText (juce::String (i + 1), cell.withY (0.0f).withHeight (13.0f).toNearestInt(),
                        juce::Justification::centred, false);
        }
    }
}

void StepGrid::paintStep (int step)
{
    proc.pattern.tracks[track].setActive (step, paintState);
    lastPainted = step;
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = stepAtX (cellArea(), steps, e.position.x);
    paintState = ! proc.pattern.tracks[track].isActive (step);
    paintStep (step);
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = stepAtX (cellArea(), steps, e.position.x);
    if (step != lastPainted && cellArea().expanded (0, 40).contains (e.position.toInt()))
        paintStep (step);
}

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const int steps = clampT (proc.trackSettings (track).steps, 1, kMaxSteps);
    const int step  = stepAtX (cellArea(), steps, e.position.x);
    if (step != hoverStep)
    {
        hoverStep = step;
        if (onHint) onHint ("Step " + juce::String (step + 1) + " - click to toggle, drag to paint");
        repaint();
    }
}

void StepGrid::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1;
    if (onHint) onHint ({});
    repaint();
}

} // namespace dy
