#include "ChainStrip.h"
#include "Theme.h"
#include "Params/Parameters.h"

namespace dy {

static juce::String patternLetter (int i) { return juce::String::charToString (static_cast<juce::juce_wchar> ('A' + i)); }

ChainStrip::ChainStrip (DYSequencerProcessor& p) : proc (p)
{
    modeButton.setClickingTogglesState (true);
    modeButton.setTooltip ("Song mode: play the chain of patterns below instead of the selected pattern. Follows the host position, so loops and jumps stay in sync.");
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::chainMode, modeButton);
    addAndMakeVisible (modeButton);

    addButton.setTooltip ("Append an entry (next pattern, same length)");
    addButton.onClick = [this] { proc.addChainEntry(); resized(); repaint(); };
    addAndMakeVisible (addButton);

    removeButton.setTooltip ("Remove the last entry");
    removeButton.onClick = [this] { proc.removeLastChainEntry(); resized(); repaint(); };
    addAndMakeVisible (removeButton);
}

juce::Rectangle<int> ChainStrip::entriesArea() const
{
    return getLocalBounds().withTrimmedLeft (84).withTrimmedRight (58).reduced (0, 3);
}

juce::Rectangle<int> ChainStrip::entryBounds (int i) const
{
    const auto a = entriesArea();
    return juce::Rectangle<int> (a.getX() + i * (kEntryW + 4), a.getY(), kEntryW, a.getHeight());
}

int ChainStrip::entryAt (juce::Point<int> p) const
{
    for (int i = 0; i < proc.chainSize(); ++i)
        if (entryBounds (i).contains (p)) return i;
    return -1;
}

void ChainStrip::refresh()
{
    removeButton.setEnabled (proc.chainSize() > 1);
    addButton.setEnabled (proc.chainSize() < kMaxChainEntries);
    repaint();
}

void ChainStrip::resized()
{
    auto r = getLocalBounds().reduced (0, 3);
    modeButton.setBounds (r.removeFromLeft (76));
    auto right = getLocalBounds().reduced (0, 3).removeFromRight (54);
    addButton.setBounds (right.removeFromLeft (25));
    right.removeFromLeft (4);
    removeButton.setBounds (right.removeFromLeft (25));
}

void ChainStrip::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    const bool on = proc.chainEnabled();
    int playingEntry = -1, barInEntry = 0;
    if (on) proc.chainPosition (playingEntry, barInEntry);

    const int n = proc.chainSize();
    for (int i = 0; i < n; ++i)
    {
        const auto e = proc.chainEntry (i);
        const auto r = entryBounds (i).toFloat();
        const bool playing = on && i == playingEntry;

        g.setColour (playing ? t.accent.withAlpha (0.25f) : (i == hoverEntry ? t.panelAlt.brighter (t.isDark ? 0.1f : 0.03f) : t.panelAlt));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (playing ? t.accent : t.outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, playing ? 1.5f : 1.0f);

        if (playing)
        {
            // progress through this entry
            g.setColour (t.accent.withAlpha (0.8f));
            const float frac = (barInEntry + 0.5f) / juce::jmax (1, e.bars);
            g.fillRoundedRectangle (r.getX() + 3.0f, r.getBottom() - 3.5f, (r.getWidth() - 6.0f) * juce::jlimit (0.0f, 1.0f, frac), 2.0f, 1.0f);
        }

        g.setColour (on ? t.text : t.textDim);
        g.setFont (uiFont (12.5f, true));
        g.drawText (patternLetter (e.pattern), r.toNearestInt().removeFromLeft (26), juce::Justification::centred);
        g.setColour (t.outline);
        g.drawVerticalLine (static_cast<int> (r.getX()) + 26, r.getY() + 5.0f, r.getBottom() - 5.0f);
        g.setColour (on ? t.text : t.textDim);
        g.setFont (uiFont (11.5f));
        g.drawText (juce::String::charToString (0x00d7) + juce::String (e.bars), r.toNearestInt().withTrimmedLeft (26), juce::Justification::centred);
    }

    g.setColour (t.textDim);
    g.setFont (uiFont (9.5f));
    const int total = proc.chainTotalBars();
    g.drawText (juce::String (total) + (total == 1 ? " bar" : " bars"), getLocalBounds().withTrimmedRight (60).removeFromRight (70),
                juce::Justification::centredRight);
}

void ChainStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = entryAt (e.position.toInt());
    if (i < 0) return;
    dragEntry = i;
    dragStartBars = proc.chainEntry (i).bars;
    dragStartY = e.position.toInt().y;
    dragged = false;
}

void ChainStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEntry < 0) return;
    const int dy = dragStartY - e.position.toInt().y;
    if (std::abs (dy) < 3 && ! dragged) return;
    dragged = true;
    auto entry = proc.chainEntry (dragEntry);
    entry.bars = juce::jlimit (1, kMaxChainEntryBars, dragStartBars + dy / 8);
    proc.setChainEntry (dragEntry, entry);
    repaint();
}

void ChainStrip::mouseUp (const juce::MouseEvent& e)
{
    if (dragEntry >= 0 && ! dragged)
    {
        auto entry = proc.chainEntry (dragEntry);
        const auto r = entryBounds (dragEntry);
        if (e.position.toInt().x < r.getX() + 26)
        {
            // letter half: cycle the pattern (shift-click goes backwards)
            entry.pattern = posMod (entry.pattern + (e.mods.isShiftDown() ? -1 : 1), kNumPatterns);
            proc.setChainEntry (dragEntry, entry);
        }
        else
        {
            // number half: click selects that pattern for editing
            proc.selectPattern (entry.pattern);
        }
        repaint();
    }
    dragEntry = -1;
}

void ChainStrip::mouseMove (const juce::MouseEvent& e)
{
    const int i = entryAt (e.position.toInt());
    if (i != hoverEntry)
    {
        hoverEntry = i;
        if (onHint) onHint (i >= 0 ? "Chain entry: click the letter to cycle the pattern (shift = back), drag the number for bars, click it to edit that pattern"
                                   : juce::String());
        repaint();
    }
}

} // namespace dy
