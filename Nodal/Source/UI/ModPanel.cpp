#include "ModPanel.h"

namespace dy::nodal {

static void paintSlot (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent)
{
    auto r = bounds.toFloat();
    g.setColour (Colours::panel2.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    g.setColour (accent);
    g.fillEllipse (12.0f, 11.0f, 6.0f, 6.0f);
    g.setFont (displayFont (10.5f));
    g.drawText (title, 24, 6, 200, 16, juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ Scope
void Scope::push (float v, bool isActive)
{
    const float prev = hist[static_cast<size_t> ((pos + static_cast<int> (hist.size()) - 1) % static_cast<int> (hist.size()))];
    hist[static_cast<size_t> (pos)] = v;
    pos = (pos + 1) % static_cast<int> (hist.size());
    still = std::abs (v - prev) < 1e-4f ? still + 1 : 0;
    if (still < static_cast<int> (hist.size()) || isActive != active)
    {
        active = isActive;
        repaint();
    }
}

void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::scopeBg);
    g.fillRoundedRectangle (r, 5.0f);
    auto plot = r.reduced (6.0f, 5.0f);

    g.setColour (Colours::line.withAlpha (0.7f));
    if (bipolar) g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
    else         g.drawHorizontalLine (juce::roundToInt (plot.getBottom()), plot.getX(), plot.getRight());

    const int n = static_cast<int> (hist.size());
    auto yOf = [&] (float v)
    {
        const float t = bipolar ? (v + 1.0f) * 0.5f : v;
        return plot.getBottom() - juce::jlimit (0.0f, 1.0f, t) * plot.getHeight();
    };
    juce::Path trace;
    for (int i = 0; i < n; ++i)
    {
        const float v = hist[static_cast<size_t> ((pos + i) % n)];
        const float x = plot.getX() + plot.getWidth() * static_cast<float> (i) / static_cast<float> (n - 1);
        if (i == 0) trace.startNewSubPath (x, yOf (v)); else trace.lineTo (x, yOf (v));
    }
    const auto col = active ? colour : Colours::dim.withAlpha (0.55f);

    juce::Path fill (trace);
    const float base = bipolar ? plot.getCentreY() : plot.getBottom();
    fill.lineTo (plot.getRight(), base);
    fill.lineTo (plot.getX(), base);
    fill.closeSubPath();
    g.setColour (col.withAlpha (0.10f));
    g.fillPath (fill);

    g.setColour (col);
    g.strokePath (trace, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float last = hist[static_cast<size_t> ((pos + n - 1) % n)];
    const juce::Point<float> head (plot.getRight(), yOf (last));
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (head.x - 6.0f, head.y - 6.0f, 12.0f, 12.0f);
    g.setColour (col);
    g.fillEllipse (head.x - 3.0f, head.y - 3.0f, 6.0f, 6.0f);
}

// ------------------------------------------------------------------ LFO slot
LfoSlot::LfoSlot (NodalProcessor& p, int i, juce::Colour a)
    : accent (a), proc (p), index (i), scope (a, true),
      rate (p.apvts, PID::lfoRate[i], "Rate"), div (p.apvts, PID::lfoDiv[i], "Division"), amount (p.apvts, PID::lfoAmount[i], "Amount"),
      shape (p.apvts, PID::lfoShape[i], "Shape"), target (p.apvts, PID::lfoTarget[i], "Target"),
      sync (p.apvts, PID::lfoSync[i], "Sync", "Free")
{
    for (auto* c : std::initializer_list<juce::Component*> { &scope, &rate, &div, &amount, &shape, &target, &sync })
        addAndMakeVisible (c);
    sync.setTooltip ("Free: rate in Hz.  Sync: locked to the song position, so it restarts identically every playback");
    target.box.setTooltip ("What this LFO moves. The knob it moves shows a ring in the LFO's colour");
    div.setVisible (false);
}

void LfoSlot::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    auto head = r.removeFromTop (18);
    sync.setBounds (head.removeFromRight (56).withSizeKeepingCentre (56, 18));
    r.removeFromTop (6);
    auto leftCol = r.removeFromLeft (152);
    scope.setBounds (leftCol.removeFromTop (50));
    leftCol.removeFromTop (5);
    const int cw = (leftCol.getWidth() - 6) / 2;
    shape.setBounds (leftCol.removeFromLeft (cw).removeFromTop (38));
    leftCol.removeFromLeft (6);
    target.setBounds (leftCol.removeFromTop (38));
    r.removeFromLeft (6);
    const int kw = r.getWidth() / 2;
    auto kr = r.removeFromLeft (kw);
    rate.setBounds (kr);
    div.setBounds (kr);
    amount.setBounds (r);
}

void LfoSlot::paint (juce::Graphics& g) { paintSlot (g, getLocalBounds(), "LFO " + juce::String (index + 1), accent); }

void LfoSlot::tick()
{
    const bool synced = proc.params.lfoSync[index]->load() > 0.5f;
    if (div.isVisible() != synced) { div.setVisible (synced); rate.setVisible (! synced); }
    const bool on = static_cast<int> (std::lround (proc.params.lfoTarget[index]->load())) != ModOff;
    scope.push ((index == 0 ? proc.telemetry.lfo1 : proc.telemetry.lfo2).load(), on);
}

// ------------------------------------------------------------------ input slot
InputSlot::InputSlot (NodalProcessor& p)
    : proc (p), scope (Colours::lilac, false),
      attack (p.apvts, PID::envAttack, "Attack"), release (p.apvts, PID::envRelease, "Release"), amount (p.apvts, PID::envAmount, "Amount"),
      target (p.apvts, PID::envTarget, "Level moves"), track (p.apvts, PID::trackPitch, "On", "Off")
{
    for (auto* c : std::initializer_list<juce::Component*> { &scope, &attack, &release, &amount, &target, &track })
        addAndMakeVisible (c);
    track.setTooltip ("Follow the pitch of the input: sing or play a note and the body retunes to it (snapped to the scale when Snap is on)");
    target.box.setTooltip ("What the input level moves: louder input pushes it by the amount");
}

void InputSlot::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromTop (24);
    pitchArea = r.removeFromRight (122);
    track.setBounds (pitchArea.getRight() - 48, 6, 48, 18);
    auto leftCol = r.removeFromLeft (136);
    scope.setBounds (leftCol.removeFromTop (50));
    leftCol.removeFromTop (5);
    target.setBounds (leftCol.removeFromTop (38));
    r.removeFromLeft (4);
    r.removeFromRight (6);
    const int kw = r.getWidth() / 3;
    attack.setBounds (r.removeFromLeft (kw));
    release.setBounds (r.removeFromLeft (kw));
    amount.setBounds (r);
}

void InputSlot::paint (juce::Graphics& g)
{
    paintSlot (g, getLocalBounds(), "INPUT FOLLOWER", accent);

    // pitch column
    auto pa = pitchArea.toFloat();
    g.setColour (Colours::line);
    g.drawVerticalLine (juce::roundToInt (pa.getX()), 8.0f, pa.getBottom());
    auto inner = pa.withTrimmedLeft (12.0f);
    g.setColour (Colours::dim);
    g.setFont (displayFont (10.5f));
    g.drawText ("PITCH", juce::Rectangle<float> (inner.getX(), 6.0f, 60.0f, 16.0f), juce::Justification::centredLeft);

    const bool on = proc.params.trackPitch->load() > 0.5f;
    const float note = shownNote, clar = shownClarity;
    auto noteRow = inner.removeFromTop (40.0f);
    g.setFont (displayFont (22.0f));
    if (on && note > 0.0f)
    {
        g.setColour (Colours::lilac);
        g.drawText (juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 3), noteRow, juce::Justification::centredLeft);
        g.setColour (Colours::dim);
        g.setFont (monoFont (10.5f));
        g.drawText (juce::String (noteToHz (note), 1) + " Hz", inner.removeFromTop (14.0f), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Colours::dim.withAlpha (0.6f));
        g.drawText (juce::String::fromUTF8 ("\xE2\x80\x94"), noteRow, juce::Justification::centredLeft);
        g.setFont (monoFont (10.5f));
        g.drawText (on ? "listening" : "off", inner.removeFromTop (14.0f), juce::Justification::centredLeft);
    }
    inner.removeFromTop (10.0f);
    g.setColour (Colours::dim);
    g.setFont (displayFont (8.5f));
    g.drawText ("CLARITY", inner.removeFromTop (12.0f), juce::Justification::centredLeft);
    auto bar = inner.removeFromTop (6.0f);
    g.setColour (Colours::scopeBg);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (on ? Colours::lilac : Colours::dim);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, on ? clar : 0.0f)), 3.0f);
    // the detector only accepts notes above 80 % clarity
    g.setColour (Colours::text.withAlpha (0.5f));
    g.fillRect (bar.getX() + bar.getWidth() * 0.8f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
}

void InputSlot::tick()
{
    const bool on = static_cast<int> (std::lround (proc.params.envTarget->load())) != ModOff;
    scope.push (proc.telemetry.env.load(), on);
    const float n = proc.telemetry.heardNote.load(), c = proc.telemetry.clarity.load();
    if (std::abs (n - shownNote) > 0.05f || std::abs (c - shownClarity) > 0.01f)
    {
        shownNote = n; shownClarity = c;
        repaint (pitchArea.expanded (0, 20));
    }
}

// ------------------------------------------------------------------ panel
ModPanel::ModPanel (NodalProcessor& p)
    : Panel ("Modulation"), lfo1 (p, 0, Colours::brass), lfo2 (p, 1, Colours::teal), input (p)
{
    addAndMakeVisible (lfo1);
    addAndMakeVisible (lfo2);
    addAndMakeVisible (input);
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (20);
    lfo1.setBounds (r.removeFromLeft (318));
    r.removeFromLeft (10);
    lfo2.setBounds (r.removeFromLeft (318));
    r.removeFromLeft (10);
    input.setBounds (r);
}

void ModPanel::tick()
{
    lfo1.tick();
    lfo2.tick();
    input.tick();
}

} // namespace dy::nodal
