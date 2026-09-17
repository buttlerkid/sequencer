#include "TrackList.h"
#include "Theme.h"
#include "Params/Parameters.h"

namespace dy {

TrackRow::TrackRow (DYSequencerProcessor& p, int i) : proc (p), index (i)
{
    onButton.setButtonText (juce::String (i + 1));
    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Track on / off");
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamIDs::track (i, ParamIDs::enabled), onButton);
    addAndMakeVisible (onButton);

    muteButton.setClickingTogglesState (true);
    muteButton.setTooltip ("Mute (keeps the clock running)");
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamIDs::track (i, ParamIDs::mute), muteButton);
    addAndMakeVisible (muteButton);

    // Clicking the buttons should also select the track.
    onButton.onClick   = [this] { if (onSelect) onSelect (index); };
    muteButton.onClick = [this] { if (onSelect) onSelect (index); };
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    onButton.setBounds (r.removeFromLeft (28));
    muteButton.setBounds (r.removeFromRight (24));
}

void TrackRow::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto s  = proc.trackSettings (index);
    auto r = getLocalBounds().toFloat();

    if (selected)
    {
        g.setColour (t.panelAlt);
        g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        g.setColour (t.accent);
        g.fillRoundedRectangle (r.removeFromLeft (3.0f).reduced (0.0f, 4.0f), 1.5f);
    }
    else if (hover)
    {
        g.setColour (t.panelAlt.withAlpha (0.5f));
        g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
    }

    auto text = getLocalBounds().reduced (4, 3);
    text.removeFromLeft (32);
    text.removeFromRight (28);

    g.setColour (s.enabled ? t.text : t.textDim);
    g.setFont (uiFont (12.0f, true));
    g.drawText ("ch " + juce::String (s.channel), text.removeFromTop (14), juce::Justification::centredLeft);

    g.setColour (t.textDim);
    g.setFont (uiFont (10.0f));
    juce::String desc = juce::String (s.steps) + " x " + kDivisionNames[clampT (static_cast<int> (std::lround (
                            proc.params.tracks[static_cast<size_t> (index)].division->load())), 0, kNumDivisions - 1)];
    if (s.pulses > 0 && s.euclidMode != EuclidOff) desc += "  E" + juce::String (s.pulses);
    g.drawText (desc, text.removeFromTop (12), juce::Justification::centredLeft);

    // Mini playhead strip
    const int cur = proc.sequencer.currentStep (index);
    auto strip = getLocalBounds().reduced (36, 0).removeFromBottom (5).withTrimmedRight (26).withTrimmedBottom (2);
    g.setColour (t.stepOff);
    g.fillRoundedRectangle (strip.toFloat(), 1.5f);
    if (cur >= 0 && s.steps > 0)
    {
        const float w = strip.getWidth() / static_cast<float> (s.steps);
        g.setColour (s.mute ? t.textDim : t.accent);
        g.fillRoundedRectangle (strip.getX() + cur * w, static_cast<float> (strip.getY()), juce::jmax (2.0f, w), static_cast<float> (strip.getHeight()), 1.5f);
    }
}

void TrackRow::mouseDown (const juce::MouseEvent&)
{
    if (onSelect) onSelect (index);
}

// ---------------------------------------------------------------------------
TrackList::TrackList (DYSequencerProcessor& p)
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto* row = rows.add (new TrackRow (p, i));
        row->onSelect = [this] (int idx) { if (onSelect) onSelect (idx); };
        addAndMakeVisible (row);
    }
}

void TrackList::setSelected (int i)
{
    for (int k = 0; k < rows.size(); ++k)
        rows[k]->setSelected (k == i);
}

void TrackList::paint (juce::Graphics& g)
{
    g.setColour (themeOf (*this).panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void TrackList::resized()
{
    auto r = getLocalBounds().reduced (2);
    const int h = r.getHeight() / kNumTracks;
    for (auto* row : rows)
        row->setBounds (r.removeFromTop (h));
}

} // namespace dy
