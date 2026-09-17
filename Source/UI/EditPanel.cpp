#include "EditPanel.h"
#include "Theme.h"
#include "Layout.h"
#include "Params/Parameters.h"

namespace dy {

EditPanel::EditPanel (DYSequencerProcessor& p) : proc (p), lanes (p)
{
    setupCombo (division,     "Rate",    kDivisionNames,        "Step length.");
    setupCombo (euclidMode,   "Euclid",  euclidModeNames(),     "Off: manual steps only. Add: manual + Euclid. Only: Euclid replaces manual steps.");
    setupCombo (pitchMode,    "Pitch",   pitchModeNames(),      "Scale: notes follow Key/Scale + Interval lane. Fixed: one note (drums).");
    juce::StringArray chans; for (int i = 1; i <= 16; ++i) chans.add (juce::String (i));
    setupCombo (channel,      "Channel", chans,                 "MIDI output channel.");
    setupCombo (swingMode,    "Shuffle", swingModeNames(),      "Global: follow the global shuffle. Off: straight. Custom: use the profile here.");
    setupCombo (swingProfile, "Profile", shuffleProfileNames(), "Per-track shuffle profile (Custom mode).");
    swingAmount.box.setTooltip ("Per-track shuffle amount (Custom mode).");
    swingAmount.label.setText ("Amount", juce::dontSendNotification);
    swingAmount.label.setFont (uiFont (10.5f));
    addAndMakeVisible (swingAmount.box);
    addAndMakeVisible (swingAmount.label);

    setupKnob (velMacro,       "Velocity", "Added to every step's velocity.");
    setupKnob (lengthMacro,    "Length",   "Scales every step's length.");
    setupKnob (shiftMacro,     "Shift",    "Moves the whole track in time (ms).");
    setupKnob (probMacro,      "Prob",     "Scales every step's probability.");
    setupKnob (repsMacro,      "Reps",     "Added to every step's repeat count.");
    setupKnob (transposeMacro, "Interval", "Transpose in scale degrees (Scale pitch mode).");

    lanes.onHint = [this] (const juce::String& h) { if (onHint) onHint (h); };
    addAndMakeVisible (lanes);

    auto steps = [this] { return proc.trackSettings (track).steps; };
    addButton (rndSteps,  "Random steps at ~50% density",                     [this, steps] { generator.randomizeSteps (proc.editPattern().tracks[track], steps(), 50); });
    addButton (rndNotes,  "Random intervals within an octave",                [this, steps] { generator.randomizeLane (proc.editPattern().tracks[track], Lane::Interval, steps(), -3, 7); });
    addButton (arpUp,     "Fill the Interval lane with a rising arpeggio",    [this, steps] { generator.arpeggiate (proc.editPattern().tracks[track], steps(), ArpMode::Up); });
    addButton (arpDown,   "Fill the Interval lane with a falling arpeggio",   [this, steps] { generator.arpeggiate (proc.editPattern().tracks[track], steps(), ArpMode::Down); });
    addButton (arpUpDown, "Rising then falling arpeggio",                     [this, steps] { generator.arpeggiate (proc.editPattern().tracks[track], steps(), ArpMode::UpDown); });
    addButton (arpRandom, "Random chord tones",                               [this, steps] { generator.arpeggiate (proc.editPattern().tracks[track], steps(), ArpMode::Random); });
    addButton (clearBtn,  "Clear steps and reset every lane of this track",   [this] { proc.clearTrack (track); });
    addButton (copyBtn,   "Copy this track (steps, lanes, settings, name)",   [this] { proc.copyTrack (track); refresh(); });
    addButton (pasteBtn,  "Paste into this track (keeps channel / mute / solo)", [this] { proc.pasteTrack (track); });

    setTrack (-1);
}

void EditPanel::setupKnob (Knob& k, const juce::String& caption, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 15);
    k.slider.setTooltip (tip);
    k.slider.setMouseDragSensitivity (140);
    addAndMakeVisible (k.slider);

    k.label.setText (caption, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (uiFont (10.5f));
    addAndMakeVisible (k.label);
}

void EditPanel::setupCombo (Combo& c, const juce::String& caption, const juce::StringArray& items, const juce::String& tip)
{
    c.box.addItemList (items, 1);
    c.box.setTooltip (tip);
    addAndMakeVisible (c.box);

    c.label.setText (caption, juce::dontSendNotification);
    c.label.setFont (uiFont (10.5f));
    addAndMakeVisible (c.label);
}

void EditPanel::addButton (juce::TextButton& b, const juce::String& tip, std::function<void()> action)
{
    b.setTooltip (tip);
    b.onClick = [this, action]
    {
        if (track < 0) return;
        action();
        if (onPatternChanged) onPatternChanged();
    };
    addAndMakeVisible (b);
}

void EditPanel::setControlsVisible (bool v)
{
    for (auto* c : getChildren())
        c->setVisible (v);
}

void EditPanel::setTrack (int i)
{
    track = i;
    if (track < 0)
    {
        setControlsVisible (false);
        repaint();
        return;
    }
    setControlsVisible (true);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    auto id = [i] (const char* suffix) { return ParamIDs::track (i, suffix); };

    // Drop the old attachments BEFORE creating new ones: a new attachment syncs its
    // control with a notification, and an old attachment still listening would write
    // that value into the previously selected track.
    for (auto* c : { &division, &euclidMode, &pitchMode, &channel, &swingMode, &swingProfile }) c->att.reset();
    swingAmount.att.reset();
    for (auto* k : { &velMacro, &lengthMacro, &shiftMacro, &probMacro, &repsMacro, &transposeMacro }) k->att.reset();

    division.att     = std::make_unique<CA> (proc.apvts, id (ParamIDs::division),      division.box);
    euclidMode.att   = std::make_unique<CA> (proc.apvts, id (ParamIDs::euclidMode),    euclidMode.box);
    pitchMode.att    = std::make_unique<CA> (proc.apvts, id (ParamIDs::pitchMode),     pitchMode.box);
    channel.att      = std::make_unique<CA> (proc.apvts, id (ParamIDs::channel),       channel.box);
    swingMode.att    = std::make_unique<CA> (proc.apvts, id (ParamIDs::swingMode),     swingMode.box);
    swingProfile.att = std::make_unique<CA> (proc.apvts, id (ParamIDs::swingProfileT), swingProfile.box);
    swingAmount.att  = std::make_unique<SA> (proc.apvts, id (ParamIDs::swingAmountT),  swingAmount.box);

    velMacro.att       = std::make_unique<SA> (proc.apvts, id (ParamIDs::velOffset),   velMacro.slider);
    lengthMacro.att    = std::make_unique<SA> (proc.apvts, id (ParamIDs::lengthScale), lengthMacro.slider);
    shiftMacro.att     = std::make_unique<SA> (proc.apvts, id (ParamIDs::shift),       shiftMacro.slider);
    probMacro.att      = std::make_unique<SA> (proc.apvts, id (ParamIDs::probScale),   probMacro.slider);
    repsMacro.att      = std::make_unique<SA> (proc.apvts, id (ParamIDs::repsAdd),     repsMacro.slider);
    transposeMacro.att = std::make_unique<SA> (proc.apvts, id (ParamIDs::transpose),   transposeMacro.slider);

    velMacro.slider.textFromValueFunction   = [] (double v) { return (v > 0 ? "+" : "") + juce::String (static_cast<int> (v)); };
    shiftMacro.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1); };
    repsMacro.slider.textFromValueFunction  = [] (double v) { return "+" + juce::String (static_cast<int> (v)); };
    transposeMacro.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (static_cast<int> (v)); };
    for (auto* k : { &velMacro, &shiftMacro, &repsMacro, &transposeMacro }) k->slider.updateText();

    pitchMode.box.onChange = [this] { refresh(); };
    swingMode.box.onChange = [this] { refresh(); };
    euclidMode.box.onChange = [this] { refresh(); };

    lanes.setTrack (i);
    refresh();
    repaint();
}

void EditPanel::refresh()
{
    if (track < 0) return;
    const auto s = proc.trackSettings (track);
    transposeMacro.slider.setEnabled (s.pitchMode == PitchScale);
    const bool custom = s.swingMode == SwingCustom;
    swingProfile.box.setEnabled (custom);
    swingAmount.box.setEnabled (custom);
    pasteBtn.setEnabled (proc.hasTrackClip());
    lanes.repaint();
    repaint();
}

void EditPanel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    if (track < 0)
    {
        g.setColour (t.textDim);
        g.setFont (uiFont (13.0f));
        g.drawText ("Select a track row or a pad to edit it", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto s = proc.trackSettings (track);
    g.setColour (t.textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("EDIT", 10, 4, 40, 16, juce::Justification::centredLeft);
    g.setColour (t.text);
    g.setFont (uiFont (12.5f, true));
    juce::String title = juce::String (track + 1) + "  " + proc.trackName (track) + "   " + proc.trackNoteName (track)
                       + "   ch " + juce::String (s.channel);
    g.drawText (title, 44, 3, 360, 18, juce::Justification::centredLeft);

    int bx = 410;
    auto badge = [&] (const juce::String& text, juce::Colour c)
    {
        g.setFont (uiFont (10.0f, true));
        const int w = 10 + text.length() * 7;
        g.setColour (c);
        g.fillRoundedRectangle (static_cast<float> (bx), 5.0f, static_cast<float> (w), 14.0f, 3.0f);
        g.setColour (juce::Colours::white);
        g.drawText (text, bx, 5, w, 14, juce::Justification::centred);
        bx += w + 6;
    };
    if (s.mute) badge ("MUTED", t.danger);
    if (s.solo) badge ("SOLO", t.solo.darker (0.2f));
    else if (proc.anyTrackSoloed()) badge ("SILENCED BY SOLO", t.textDim);

    g.setColour (t.textDim);
    g.setFont (uiFont (10.5f, true));
    g.drawText ("LANE MACROS", getLocalBounds().withTrimmedBottom (26 + 8).removeFromBottom (74).withX (12).withWidth (100).withHeight (12),
                juce::Justification::centredLeft);
}

void EditPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromTop (22);                                // title row

    // Settings row
    auto settings = r.removeFromTop (38);
    const int comboW = juce::jmin (130, (settings.getWidth() - 70) / 6);
    auto placeCombo = [&] (Combo& c)
    {
        auto col = settings.removeFromLeft (comboW).reduced (3, 0);
        c.label.setBounds (col.removeFromTop (13));
        c.box.setBounds (col.removeFromTop (22));
    };
    placeCombo (division);
    placeCombo (euclidMode);
    placeCombo (pitchMode);
    placeCombo (channel);
    placeCombo (swingMode);
    placeCombo (swingProfile);
    {
        auto col = settings.removeFromLeft (64).reduced (3, 0);
        swingAmount.label.setBounds (col.removeFromTop (13));
        swingAmount.box.setBounds (col.removeFromTop (22));
    }
    r.removeFromTop (6);

    // Generators at the very bottom, macros above them, the lane editor takes the rest.
    auto buttons = r.removeFromBottom (24);
    juce::TextButton* order[] = { &rndSteps, &rndNotes, &arpUp, &arpDown, &arpUpDown, &arpRandom, &clearBtn, &copyBtn, &pasteBtn };
    const int bw = buttons.getWidth() / static_cast<int> (std::size (order));
    for (auto* b : order)
        b->setBounds (buttons.removeFromLeft (bw).reduced (2, 1));
    r.removeFromBottom (4);

    auto macros = r.removeFromBottom (74);
    macros.removeFromLeft (100);                          // "LANE MACROS" caption
    Knob* knobs[] = { &velMacro, &lengthMacro, &shiftMacro, &probMacro, &repsMacro, &transposeMacro };
    const int kw = juce::jmin (84, macros.getWidth() / 6);
    for (auto* k : knobs)
    {
        auto col = macros.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (13));
        k->slider.setBounds (col);
    }
    r.removeFromBottom (4);

    lanes.setBounds (r);
}

} // namespace dy
