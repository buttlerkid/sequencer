#include "PadsPanel.h"
#include "Theme.h"

namespace dy {

PadsPanel::PadsPanel (DYSequencerProcessor& p) : proc (p)
{
    layoutButton.setTooltip ("Assign notes to all 16 tracks from a preset layout");
    layoutButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Pad layout");
        m.addItem (1, "GM Drums (kick, snare, hats...)");
        m.addItem (2, "Chromatic from C1 (Drum Rack)");
        m.addItem (3, "Melodic (all tracks follow Key / Scale)");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&layoutButton), [this] (int r)
        {
            if (r == 0) return;
            proc.applyPadLayout (static_cast<PadLayout> (r - 1));
            if (onLayoutApplied) onLayoutApplied();
            repaint();
        });
    };
    addAndMakeVisible (layoutButton);
}

void PadsPanel::resized()
{
    layoutButton.setBounds (getLocalBounds().removeFromTop (28).removeFromRight (74).reduced (4, 4));
}

juce::Rectangle<float> PadsPanel::padBounds (int i) const
{
    const auto a = padArea().toFloat();
    const float w = a.getWidth() / 4.0f, h = a.getHeight() / 4.0f;
    const int col = i % 4, row = i / 4;
    return juce::Rectangle<float> (a.getX() + col * w, a.getY() + row * h, w, h).reduced (2.5f);
}

int PadsPanel::padAt (juce::Point<float> p) const
{
    for (int i = 0; i < kNumTracks; ++i)
        if (padBounds (i).contains (p)) return i;
    return -1;
}

void PadsPanel::refresh()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto hits = proc.hitCount[static_cast<size_t> (i)].load (std::memory_order_relaxed);
        if (hits != lastHits[static_cast<size_t> (i)])
        {
            lastHits[static_cast<size_t> (i)] = hits;
            flashUntil[static_cast<size_t> (i)] = now + 120.0;
        }
    }
    repaint();
}

void PadsPanel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (t.textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("PADS", 10, 4, 100, 20, juce::Justification::centredLeft);

    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool anySolo = proc.anyTrackSoloed();

    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto s = proc.trackSettings (i);
        const auto r = padBounds (i);
        const bool flashing = now < flashUntil[static_cast<size_t> (i)];
        const bool silenced = s.mute || (anySolo && ! s.solo);

        juce::Colour fill = s.enabled ? t.panelAlt : t.panelAlt.withAlpha (0.35f);
        if (flashing) fill = t.accent.withAlpha (s.enabled ? 0.9f : 0.3f);
        else if (i == hoverPad) fill = fill.brighter (t.isDark ? 0.12f : 0.04f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (i == selected ? t.accent : t.outline.withAlpha (s.enabled ? 1.0f : 0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, i == selected ? 1.8f : 1.0f);

        const auto textCol = flashing ? (t.isDark ? juce::Colour (0xff15171c) : juce::Colours::white)
                                      : (s.enabled ? t.text : t.textDim.withAlpha (0.6f));
        if (! s.enabled)
        {
            g.setColour (t.textDim.withAlpha (0.35f));
            g.setFont (uiFont (18.0f));
            g.drawText ("+", r.toNearestInt(), juce::Justification::centred);
            g.setFont (uiFont (9.5f));
            g.drawText (juce::String (i + 1), r.reduced (5.0f, 3.0f).toNearestInt(), juce::Justification::topLeft);
            continue;
        }

        g.setColour (textCol);
        g.setFont (uiFont (12.0f, true));
        g.drawText (proc.trackNoteName (i), r.reduced (5.0f, 3.0f).toNearestInt(), juce::Justification::topLeft);
        g.setColour (textCol.withAlpha (0.6f));
        g.setFont (uiFont (9.5f));
        g.drawText (juce::String (i + 1), r.reduced (5.0f, 3.0f).toNearestInt(), juce::Justification::topRight);
        g.setFont (uiFont (10.0f));
        g.setColour (textCol.withAlpha (0.8f));
        g.drawFittedText (proc.trackName (i), r.reduced (5.0f, 3.0f).toNearestInt(), juce::Justification::bottomLeft, 1);

        if (s.enabled && silenced)
        {
            g.setColour (s.mute ? t.danger : t.textDim);
            g.fillEllipse (r.getRight() - 11.0f, r.getY() + 5.0f, 6.0f, 6.0f);
        }
        else if (s.enabled && s.solo)
        {
            g.setColour (t.solo);
            g.fillEllipse (r.getRight() - 11.0f, r.getY() + 5.0f, 6.0f, 6.0f);
        }
    }
}

void PadsPanel::mouseDown (const juce::MouseEvent& e)
{
    const int i = padAt (e.position);
    if (i >= 0 && onPadClick) onPadClick (i);
}

void PadsPanel::mouseMove (const juce::MouseEvent& e)
{
    const int i = padAt (e.position);
    if (i != hoverPad) { hoverPad = i; repaint(); }
}

void PadsPanel::mouseExit (const juce::MouseEvent&)
{
    hoverPad = -1;
    repaint();
}

} // namespace dy
