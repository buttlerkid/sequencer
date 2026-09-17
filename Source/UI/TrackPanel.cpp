#include "TrackPanel.h"
#include "Theme.h"
#include "Params/Parameters.h"

namespace dy {

TrackPanel::TrackPanel (DYSequencerProcessor& p) : proc (p)
{
    setupKnob (steps,       "Steps",     "Pattern length in steps (1-64). Tracks of different lengths run as polymeters.");
    setupKnob (pulses,      "Pulses",    "Euclidean hits spread across the steps. 0 = off.");
    setupKnob (rotate,      "Rotate",    "Rotate the Euclidean pattern.");
    setupKnob (transpose,   "Transpose", "Offset in scale degrees (7 = one octave in a 7-note scale).");
    setupKnob (fixedNote,   "Note",      "Fixed MIDI note (drum mode).");
    setupKnob (swingAmount, "Swing",     "Per-track swing amount (Custom swing mode only).");

    setupCombo (division,     "Rate",    kDivisionNames,       "Step length.");
    setupCombo (euclidMode,   "Euclid",  euclidModeNames(),    "Off: manual steps only. Add: manual + Euclid. Only: Euclid replaces manual steps.");
    setupCombo (pitchMode,    "Pitch",   pitchModeNames(),     "Scale: notes follow Key/Scale + Interval lane. Fixed: one note (drums).");
    juce::StringArray chans; for (int i = 1; i <= 16; ++i) chans.add (juce::String (i));
    setupCombo (channel,      "Channel", chans,                "MIDI output channel.");
    setupCombo (swingMode,    "Shuffle", swingModeNames(),     "Global: follow the header swing. Off: straight. Custom: use the profile below.");
    setupCombo (swingProfile, "Profile", shuffleProfileNames(), "Per-track shuffle profile (Custom mode).");

    addButton (rndSteps,  "Random steps at ~50% density",                  [this] { generator.randomizeSteps (proc.pattern.tracks[track], proc.trackSettings (track).steps, 50); });
    addButton (rndNotes,  "Random intervals within an octave",             [this] { generator.randomizeLane (proc.pattern.tracks[track], Lane::Interval, proc.trackSettings (track).steps, -3, 7); });
    addButton (arpUp,     "Fill the Interval lane with a rising arpeggio", [this] { generator.arpeggiate (proc.pattern.tracks[track], proc.trackSettings (track).steps, ArpMode::Up); });
    addButton (arpDown,   "Fill the Interval lane with a falling arpeggio",[this] { generator.arpeggiate (proc.pattern.tracks[track], proc.trackSettings (track).steps, ArpMode::Down); });
    addButton (arpUpDown, "Rising then falling arpeggio",                  [this] { generator.arpeggiate (proc.pattern.tracks[track], proc.trackSettings (track).steps, ArpMode::UpDown); });
    addButton (arpRandom, "Random chord tones",                            [this] { generator.arpeggiate (proc.pattern.tracks[track], proc.trackSettings (track).steps, ArpMode::Random); });
    addButton (clearBtn,  "Clear steps and reset every lane",              [this] { proc.clearTrack (track); });
    addButton (copyBtn,   "Copy this track (steps, lanes, settings)",      [this] { proc.copyTrack (track); refresh(); });
    addButton (pasteBtn,  "Paste into this track (keeps channel / on / mute)", [this] { proc.pasteTrack (track); });

    setTrack (0);
}

void TrackPanel::setupKnob (Knob& k, const juce::String& caption, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    k.slider.setTooltip (tip);
    k.slider.setMouseDragSensitivity (120);
    addAndMakeVisible (k.slider);

    k.label.setText (caption, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (uiFont (11.0f));
    addAndMakeVisible (k.label);
}

void TrackPanel::setupCombo (Combo& c, const juce::String& caption, const juce::StringArray& items, const juce::String& tip)
{
    c.box.addItemList (items, 1);
    c.box.setTooltip (tip);
    addAndMakeVisible (c.box);

    c.label.setText (caption, juce::dontSendNotification);
    c.label.setFont (uiFont (11.0f));
    addAndMakeVisible (c.label);
}

void TrackPanel::addButton (juce::TextButton& b, const juce::String& tip, std::function<void()> action)
{
    b.setTooltip (tip);
    b.onClick = [this, action]
    {
        action();
        if (onPatternChanged) onPatternChanged();
    };
    addAndMakeVisible (b);
}

void TrackPanel::setTrack (int i)
{
    track = i;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    auto id = [i] (const char* suffix) { return ParamIDs::track (i, suffix); };

    steps.att       = std::make_unique<SA> (proc.apvts, id (ParamIDs::steps),        steps.slider);
    pulses.att      = std::make_unique<SA> (proc.apvts, id (ParamIDs::pulses),       pulses.slider);
    rotate.att      = std::make_unique<SA> (proc.apvts, id (ParamIDs::rotate),       rotate.slider);
    transpose.att   = std::make_unique<SA> (proc.apvts, id (ParamIDs::transpose),    transpose.slider);
    fixedNote.att   = std::make_unique<SA> (proc.apvts, id (ParamIDs::fixedNote),    fixedNote.slider);
    swingAmount.att = std::make_unique<SA> (proc.apvts, id (ParamIDs::swingAmountT), swingAmount.slider);

    division.att     = std::make_unique<CA> (proc.apvts, id (ParamIDs::division),      division.box);
    euclidMode.att   = std::make_unique<CA> (proc.apvts, id (ParamIDs::euclidMode),    euclidMode.box);
    pitchMode.att    = std::make_unique<CA> (proc.apvts, id (ParamIDs::pitchMode),     pitchMode.box);
    channel.att      = std::make_unique<CA> (proc.apvts, id (ParamIDs::channel),       channel.box);
    swingMode.att    = std::make_unique<CA> (proc.apvts, id (ParamIDs::swingMode),     swingMode.box);
    swingProfile.att = std::make_unique<CA> (proc.apvts, id (ParamIDs::swingProfileT), swingProfile.box);

    // Pitch mode / swing mode change what makes sense to show.
    pitchMode.box.onChange = [this] { refresh(); };
    swingMode.box.onChange = [this] { refresh(); };
    refresh();
    repaint();
}

void TrackPanel::refresh()
{
    const auto s = proc.trackSettings (track);
    const bool scaleMode = s.pitchMode == PitchScale;
    transpose.slider.setVisible (scaleMode);
    transpose.label.setVisible (scaleMode);
    fixedNote.slider.setVisible (! scaleMode);
    fixedNote.label.setVisible (! scaleMode);

    const bool custom = s.swingMode == SwingCustom;
    swingProfile.box.setEnabled (custom);
    swingAmount.slider.setEnabled (custom);

    const bool euclidOn = s.euclidMode != EuclidOff;
    pulses.slider.setEnabled (euclidOn);
    rotate.slider.setEnabled (euclidOn);

    pasteBtn.setEnabled (proc.hasTrackClip());
}

void TrackPanel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    g.setColour (t.textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("TRACK " + juce::String (track + 1), 10, 4, 100, 14, juce::Justification::centredLeft);
}

void TrackPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromTop (12);

    auto buttons = r.removeFromBottom (26);
    auto controls = r.withTrimmedBottom (4);

    // Knobs on the left: caption above, knob + textbox below.
    const int knobW = 66;
    auto placeKnob = [&] (Knob& k)
    {
        auto col = controls.removeFromLeft (knobW);
        k.label.setBounds (col.removeFromTop (14));
        k.slider.setBounds (col);
    };
    placeKnob (steps);
    placeKnob (pulses);
    placeKnob (rotate);
    // transpose and fixedNote share a slot
    {
        auto col = controls.removeFromLeft (knobW);
        transpose.label.setBounds (col.withHeight (14));
        fixedNote.label.setBounds (col.withHeight (14));
        auto knobArea = col.withTrimmedTop (14);
        transpose.slider.setBounds (knobArea);
        fixedNote.slider.setBounds (knobArea);
    }
    placeKnob (swingAmount);

    controls.removeFromLeft (10);

    // Combos in two rows of three.
    const int comboW = juce::jmax (90, controls.getWidth() / 3);
    auto rowA = controls.removeFromTop (controls.getHeight() / 2);
    auto rowB = controls;
    auto placeCombo = [&] (Combo& c, juce::Rectangle<int>& row)
    {
        auto col = row.removeFromLeft (comboW).reduced (4, 2);
        c.label.setBounds (col.removeFromTop (14));
        c.box.setBounds (col.removeFromTop (24));
    };
    placeCombo (division, rowA);
    placeCombo (euclidMode, rowA);
    placeCombo (pitchMode, rowA);
    placeCombo (channel, rowB);
    placeCombo (swingMode, rowB);
    placeCombo (swingProfile, rowB);

    juce::TextButton* order[] = { &rndSteps, &rndNotes, &arpUp, &arpDown, &arpUpDown, &arpRandom, &clearBtn, &copyBtn, &pasteBtn };
    const int bw = buttons.getWidth() / static_cast<int> (std::size (order));
    for (auto* b : order)
        b->setBounds (buttons.removeFromLeft (bw).reduced (2, 1));
}

} // namespace dy
