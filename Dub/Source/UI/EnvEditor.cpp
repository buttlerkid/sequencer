#include "EnvEditor.h"
#include "NodalLookAndFeel.h"

namespace dy::dub {

namespace Colours = dy::nodal::Colours;
using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

// field offsets from VRise / FRise
enum { kRise = 0, kFall = 1, kSus = 2, kCurve = 3 };

EnvEditor::EnvEditor (DubProcessor& p) : proc (p)
{
    addAndMakeVisible (allSteps);
    addAndMakeVisible (allBanks);
    allSteps.setTooltip ("Give every step of this bank these two envelopes");
    allBanks.setTooltip ("Give every step of both banks these two envelopes");
    allSteps.onClick = [this] { copyTo (false); };
    allBanks.onClick = [this] { copyTo (true); };
}

int EnvEditor::blockStart() const
{
    int s = clampT (step, 0, kSteps - 1);
    while (s > 0 && proc.steps.get (bank, s - 1, StepStore::Chain) > 0.5f) --s;
    return s;
}

int EnvEditor::blockLen() const
{
    const int len = static_cast<int> (proc.apvts.getRawParameterValue (PID::length)->load());
    int e = blockStart();
    while (e < len - 1 && proc.steps.get (bank, e, StepStore::Chain) > 0.5f) ++e;
    return e - blockStart() + 1;
}

void EnvEditor::resized()
{
    auto r = getLocalBounds().removeFromBottom (26);
    const int w = (r.getWidth() - 8) / 2;
    allSteps.setBounds (r.removeFromLeft (w));
    r.removeFromLeft (8);
    allBanks.setBounds (r);
}

juce::Rectangle<float> EnvEditor::graph (int which) const
{
    auto r = getLocalBounds().toFloat().withTrimmedTop (22.0f).withTrimmedBottom (34.0f);
    const float h = (r.getHeight() - 22.0f) / 2.0f;
    auto g = which == 0 ? r.removeFromTop (h) : r.withTrimmedTop (h + 22.0f);
    return g.reduced (8.0f, 14.0f).withTrimmedTop (12.0f);
}

juce::Point<float> EnvEditor::node (int which, int n) const
{
    const auto g = graph (which);
    const int s = blockStart(), f = fieldBase (which);
    if (n == 0) return { g.getX() + proc.steps.get (bank, s, f + kRise) * g.getWidth(), g.getY() };
    const float fall = proc.steps.get (bank, s, f + kFall), sus = proc.steps.get (bank, s, f + kSus);
    return { g.getX() + (1.0f - fall) * g.getWidth(), g.getBottom() - sus * g.getHeight() };
}

void EnvEditor::paint (juce::Graphics& g)
{
    const int s = blockStart(), n = blockLen();
    g.setColour (Colours::dim);
    g.setFont (displayFont (10.5f));
    g.drawText ("ENVELOPE", 12, 6, 120, 14, juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.setFont (monoFont (11.0f));
    const juce::String which = "bank " + juce::String (bank == 0 ? "A" : "B") + "  step " + juce::String (s + 1)
                             + (n > 1 ? "-" + juce::String (s + n) + " (chained)" : juce::String());
    g.drawText (which, 110, 5, getWidth() - 122, 16, juce::Justification::centredRight);

    const auto& tel = proc.telemetry();
    const bool playing = tel.running.load() && tel.step.load() >= 0 && tel.bank.load() == bank && tel.blockStart.load() == s;

    for (int w = 0; w < 2; ++w)
    {
        auto r = graph (w);
        auto frame = r.expanded (8.0f, 12.0f).withTrimmedTop (-12.0f);
        g.setColour (Colours::scopeBg);
        g.fillRoundedRectangle (frame, 6.0f);
        g.setColour (Colours::dim);
        g.setFont (displayFont (9.0f));
        g.drawText (w == 0 ? "VOLUME" : "FILTER", frame.reduced (8.0f, 2.0f).removeFromTop (12.0f), juce::Justification::centredLeft);

        const int f = fieldBase (w);
        const Env env { proc.steps.get (bank, s, f + kRise), proc.steps.get (bank, s, f + kFall),
                        proc.steps.get (bank, s, f + kSus), proc.steps.get (bank, s, f + kCurve) };
        g.setFont (monoFont (9.5f));
        g.drawText ("rise " + juce::String (juce::roundToInt (env.rise * 100)) + "%  fall " + juce::String (juce::roundToInt (env.fall * 100))
                    + "%  sus " + juce::String (juce::roundToInt (env.sustain * 100)) + "%  curve " + juce::String (env.curve, 2),
                    frame.reduced (8.0f, 2.0f).removeFromTop (12.0f), juce::Justification::centredRight);

        // step divisions inside a chained block
        g.setColour (Colours::line.withAlpha (0.6f));
        for (int k = 1; k < n; ++k) g.drawVerticalLine (juce::roundToInt (r.getX() + r.getWidth() * k / n), r.getY(), r.getBottom());

        juce::Path p;
        const int pts = 120;
        for (int i = 0; i <= pts; ++i)
        {
            const float ph = i / static_cast<float> (pts);
            const float x = r.getX() + ph * r.getWidth(), y = r.getBottom() - env.at (ph) * r.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        const auto col = w == 0 ? Colours::brass : Colours::teal;
        juce::Path fill (p);
        fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.setColour (col.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (1.8f));

        for (int k = 0; k < 2; ++k)
        {
            const auto pt = node (w, k);
            g.setColour (Colours::page);
            g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
            g.setColour (col);
            g.drawEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f, 1.6f);
        }

        if (playing)
        {
            const float ph = tel.blockPhase.load();
            const float x = r.getX() + ph * r.getWidth(), y = r.getBottom() - env.at (ph) * r.getHeight();
            g.setColour (Colours::text);
            g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
    }
}

void EnvEditor::mouseDown (const juce::MouseEvent& e)
{
    drag = DragNone;
    for (int w = 0; w < 2; ++w)
    {
        if (! graph (w).expanded (10.0f, 10.0f).contains (e.position)) continue;
        dragWhich = w;
        if (e.position.getDistanceFrom (node (w, 1)) < 10.0f) drag = DragSus;
        else if (e.position.getDistanceFrom (node (w, 0)) < 10.0f) drag = DragRise;
        else
        {
            drag = DragCurve;
            startCurve = proc.steps.get (bank, blockStart(), fieldBase (w) + kCurve);
            startY = e.position.y;
        }
    }
}

void EnvEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == DragNone) return;
    const auto r = graph (dragWhich);
    const int s = blockStart(), f = fieldBase (dragWhich);
    auto& st = proc.steps;
    const float x = clampT ((e.position.x - r.getX()) / r.getWidth(), 0.0f, 1.0f);
    if (drag == DragRise)
        st.set (bank, s, f + kRise, std::min (x, 1.0f - st.get (bank, s, f + kFall)));
    else if (drag == DragSus)
    {
        st.set (bank, s, f + kFall, clampT (1.0f - x, 0.0f, 1.0f - st.get (bank, s, f + kRise)));
        st.set (bank, s, f + kSus, clampT ((r.getBottom() - e.position.y) / r.getHeight(), 0.0f, 1.0f));
    }
    else
        st.set (bank, s, f + kCurve, clampT (startCurve + (e.position.y - startY) / 90.0f, -1.0f, 1.0f));
    repaint();
}

void EnvEditor::mouseMove (const juce::MouseEvent& e)
{
    for (int w = 0; w < 2; ++w)
        if (graph (w).expanded (10.0f, 10.0f).contains (e.position))
        {
            if (e.position.getDistanceFrom (node (w, 1)) < 10.0f) { setTooltip ("Drag: where the fall starts, and the sustain level"); setMouseCursor (juce::MouseCursor::DraggingHandCursor); return; }
            if (e.position.getDistanceFrom (node (w, 0)) < 10.0f) { setTooltip ("Drag sideways: rise time"); setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); return; }
            setTooltip ("Drag up / down to bend the curve, double-click to reset this envelope");
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            return;
        }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void EnvEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int w = 0; w < 2; ++w)
        if (graph (w).expanded (10.0f, 10.0f).contains (e.position))
        {
            const Step def;
            const Env& d = w == 0 ? def.venv : def.fenv;
            const int s = blockStart(), f = fieldBase (w);
            proc.steps.set (bank, s, f + kRise, d.rise);
            proc.steps.set (bank, s, f + kFall, d.fall);
            proc.steps.set (bank, s, f + kSus, d.sustain);
            proc.steps.set (bank, s, f + kCurve, d.curve);
            repaint();
        }
}

void EnvEditor::copyTo (bool bothBanks)
{
    const int s = blockStart();
    float vals[8];
    for (int k = 0; k < 4; ++k) { vals[k] = proc.steps.get (bank, s, StepStore::VRise + k); vals[4 + k] = proc.steps.get (bank, s, StepStore::FRise + k); }
    for (int b = 0; b < kBanks; ++b)
    {
        if (! bothBanks && b != bank) continue;
        for (int t = 0; t < kSteps; ++t)
            for (int k = 0; k < 4; ++k)
            {
                proc.steps.set (b, t, StepStore::VRise + k, vals[k]);
                proc.steps.set (b, t, StepStore::FRise + k, vals[4 + k]);
            }
    }
    repaint();
}

} // namespace dy::dub
