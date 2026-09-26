#include "PlayPanel.h"

namespace dy::nodal {

// Glyphs for the exciter buttons.
static void drawExciterIcon (juce::Graphics& g, juce::Rectangle<float> r, int exc, bool on)
{
    const auto c = r.getCentre();
    const float s = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path p;
    switch (exc)
    {
        case ExcMallet:     // a stick with a round head
            p.startNewSubPath (c.x - s * 0.75f, c.y + s * 0.75f);
            p.lineTo (c.x + s * 0.2f, c.y - s * 0.2f);
            p.addEllipse (c.x + s * 0.05f, c.y - s * 0.85f, s * 0.8f, s * 0.8f);
            break;
        case ExcPluck:      // a string pulled sideways
            p.startNewSubPath (c.x - s * 0.9f, c.y - s * 0.55f);
            p.lineTo (c.x + s * 0.1f, c.y + s * 0.35f);
            p.lineTo (c.x + s * 0.9f, c.y - s * 0.55f);
            p.addTriangle (c.x + s * 0.1f, c.y + s * 0.35f, c.x - s * 0.15f, c.y + s * 0.85f, c.x + s * 0.35f, c.y + s * 0.85f);
            break;
        case ExcBow:        // bow stick and hair
            p.startNewSubPath (c.x - s * 0.9f, c.y + s * 0.2f);
            p.quadraticTo (c.x, c.y - s * 0.75f, c.x + s * 0.9f, c.y + s * 0.2f);
            p.startNewSubPath (c.x - s * 0.9f, c.y + s * 0.2f);
            p.lineTo (c.x + s * 0.9f, c.y + s * 0.2f);
            p.startNewSubPath (c.x - s * 0.5f, c.y + s * 0.65f);
            p.lineTo (c.x + s * 0.5f, c.y + s * 0.65f);
            break;
        case ExcNoise:      // a burst of random steps
        {
            Rng rng (7);
            p.startNewSubPath (c.x - s * 0.9f, c.y);
            for (int i = 1; i <= 12; ++i)
                p.lineTo (c.x - s * 0.9f + s * 1.8f * i / 12.0f, c.y + rng.bipolar() * s * 0.7f * std::sin (static_cast<float> (kPi) * i / 12.0f));
            break;
        }
        default:            // input: a wave going into the body
            p.startNewSubPath (c.x - s * 0.9f, c.y);
            for (int i = 1; i <= 20; ++i)
                p.lineTo (c.x - s * 0.9f + s * 1.2f * i / 20.0f, c.y + std::sin (i * 0.9f) * s * 0.45f);
            p.addTriangle (c.x + s * 0.35f, c.y - s * 0.35f, c.x + s * 0.35f, c.y + s * 0.35f, c.x + s * 0.9f, c.y);
            break;
    }
    g.setColour (on ? Colours::brass : Colours::text.withAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static std::vector<int> exciterChoices()
{
    if constexpr (kInstrumentBuild) return { ExcMallet, ExcPluck, ExcBow, ExcNoise };
    return { ExcMallet, ExcPluck, ExcBow, ExcNoise, ExcInput };
}

static const char* exciterHelpText (int e)
{
    switch (e)
    {
        case ExcMallet: return "A felt or hard mallet. Tone: soft to hard.";
        case ExcPluck:  return "A short bright pluck. Tone: warm to sharp.";
        case ExcBow:    return "Bowed while the key is down. Attack and release shape it.";
        case ExcNoise:  return "Breath-like noise while the key is down.";
        default:        return "The audio input sounds each note while its key is down.";
    }
}

static const char* playHelpText (int m)
{
    switch (m)
    {
        case PlayEffect:    return "The input rings the body. MIDI is ignored.";
        case PlayKeyFollow: return "The input rings the body; MIDI notes retune it.";
        default:            return kInstrumentBuild ? "MIDI notes play the body with the exciter."
                                                    : "MIDI notes play the body with the exciter. Route a MIDI track to it in Live.";
    }
}

PlayPanel::PlayPanel (NodalProcessor& p)
    : proc (p),
      playMode (p.apvts, PID::playMode),
      exciter (p.apvts, PID::exciter, exciterChoices(), drawExciterIcon),
      sidechain (p.apvts, PID::sidechain),
      tone (p.apvts, PID::excTone, "Tone"), attack (p.apvts, PID::excAttack, "Attack"), release (p.apvts, PID::excRelease, "Release"),
      damp (p.apvts, PID::noteDamp, "Key-up"), velocity (p.apvts, PID::velSens, "Velocity"), voices (p.apvts, PID::polyphony, "Voices")
{
    for (auto* c : std::initializer_list<juce::Component*> { &exciter, &tone, &attack, &release, &damp, &velocity, &voices, &playHelp, &exciterHelp })
        addAndMakeVisible (c);
    if constexpr (! kInstrumentBuild)
    {
        addAndMakeVisible (playMode);
        addAndMakeVisible (sidechain);
    }
    for (auto* l : { &playHelp, &exciterHelp })
    {
        l->setFont (bodyFont (11.5f));
        l->setColour (juce::Label::textColourId, Colours::dim);
        l->setJustificationType (juce::Justification::topLeft);
        l->setMinimumHorizontalScale (0.8f);
    }
    damp.slider.setTooltip ("How much letting go of a key stops the ring: 0 rings on like a bell, 100 % stops it like a damped string");
    voices.slider.setTooltip ("Voices: 1 is mono, gliding between held notes (Glide sets the time)");
    sidechain.setTooltip ("Excites: the sidechain rings the body (drums on the sidechain, a pad through it). "
                          "Envelope: the input follower listens to the sidechain");
    ledNote.fill (-2);
    updateHelp();
}

void PlayPanel::resized()
{
    auto r = getLocalBounds();
    playSlot = r.removeFromLeft (262);
    r.removeFromLeft (10);
    excSlot = r.removeFromLeft (392);
    r.removeFromLeft (10);
    voiceSlot = r;

    {
        auto a = playSlot.reduced (10, 6);
        a.removeFromTop (20);
        if constexpr (kInstrumentBuild)
            playHelp.setBounds (a.removeFromTop (60));
        else
        {
            playMode.setBounds (a.removeFromTop (26));
            a.removeFromTop (4);
            playHelp.setBounds (a.removeFromTop (30));
            a.removeFromTop (2);
            auto row = a.removeFromTop (24);
            row.removeFromLeft (78);                     // "SIDECHAIN" caption
            sidechain.setBounds (row);
        }
    }
    {
        auto a = excSlot.reduced (10, 6);
        a.removeFromTop (20);
        exciter.setBounds (a.removeFromTop (72));
        a.removeFromTop (4);
        exciterHelp.setBounds (a.removeFromTop (18));
    }
    {
        auto a = voiceSlot.reduced (10, 6);
        auto head = a.removeFromTop (20);
        leds = head.removeFromRight (8 * 40).translated (0, 0);
        a.removeFromTop (4);
        const int kw = a.getWidth() / 6;
        for (auto* k : { &tone, &attack, &release, &damp, &velocity, &voices })
            k->setBounds (a.removeFromLeft (kw));
    }
}

void PlayPanel::paint (juce::Graphics& g)
{
    auto slot = [&] (juce::Rectangle<int> b, const juce::String& title)
    {
        auto r = b.toFloat();
        g.setColour (Colours::panel2.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (title, b.getX() + 12, b.getY() + 6, 200, 16, juce::Justification::centredLeft);
    };
    slot (playSlot, kInstrumentBuild ? "INSTRUMENT" : "PLAY");
    slot (excSlot, "EXCITER");
    slot (voiceSlot, "VOICES");

    if constexpr (! kInstrumentBuild)
    {
        const auto sb = sidechain.getBounds();
        g.setColour (Colours::dim);
        g.setFont (displayFont (9.5f));
        g.drawText ("SIDECHAIN", sb.getX() - 78, sb.getY(), 74, sb.getHeight(), juce::Justification::centredLeft);
        // connected or not
        g.setColour (shownSc ? Colours::teal : Colours::line);
        g.fillEllipse (static_cast<float> (playSlot.getRight() - 20), static_cast<float> (playSlot.getY() + 10), 7.0f, 7.0f);
    }

    // voice lights: note name while ringing, brighter when louder, outlined while held
    for (int v = 0; v < kMaxVoices; ++v)
    {
        auto c = juce::Rectangle<float> (static_cast<float> (leds.getX() + v * 40), static_cast<float> (leds.getY() + 1), 36.0f, 16.0f);
        const int note = ledNote[static_cast<size_t> (v)];
        const bool usable = v < static_cast<int> (std::lround (proc.params.polyphony->load()));
        g.setColour (note >= 0 ? Colours::brass.withAlpha (0.18f + 0.6f * ledLevel[static_cast<size_t> (v)]) : Colours::scopeBg);
        g.fillRoundedRectangle (c, 4.0f);
        g.setColour (ledHeld[static_cast<size_t> (v)] && note >= 0 ? Colours::brass : Colours::line.withAlpha (usable ? 1.0f : 0.4f));
        g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.0f);
        if (note >= 0)
        {
            g.setColour (Colours::text);
            g.setFont (monoFont (10.0f));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), c, juce::Justification::centred);
        }
    }
}

void PlayPanel::updateHelp()
{
    const int m = static_cast<int> (std::lround (proc.params.playMode->load()));
    const int e = static_cast<int> (std::lround (proc.params.exciter->load()));
    if (m != shownMode) { shownMode = m; playHelp.setText (playHelpText (m), juce::dontSendNotification); }
    if (e != shownExc)
    {
        shownExc = e;
        exciterHelp.setText (exciterHelpText (e), juce::dontSendNotification);
        const bool sustained = e >= ExcBow;
        attack.setEnabled (sustained);
        release.setEnabled (sustained);
    }
    // the exciter only matters for instrument notes
    const bool playing = m == PlayInstrument;
    exciter.setAlpha (playing ? 1.0f : 0.45f);
    for (auto* k : { &tone, &velocity, &voices, &damp }) k->setAlpha (playing ? 1.0f : 0.45f);
}

void PlayPanel::tick()
{
    updateHelp();
    bool dirty = false;
    for (int v = 0; v < kMaxVoices; ++v)
    {
        const size_t k = static_cast<size_t> (v);
        const int note = proc.telemetry.voiceNote[k].load();
        const float lev = std::round (proc.telemetry.voiceLevel[k].load() * 20.0f) / 20.0f;
        const bool held = proc.telemetry.voiceHeld[k].load();
        if (note != ledNote[k] || lev != ledLevel[k] || held != ledHeld[k])
        {
            ledNote[k] = note; ledLevel[k] = lev; ledHeld[k] = held;
            dirty = true;
        }
    }
    if (dirty) repaint (leds.expanded (2));
    const bool sc = proc.telemetry.sidechainConnected.load();
    if (sc != shownSc) { shownSc = sc; repaint (playSlot); }
}

} // namespace dy::nodal
