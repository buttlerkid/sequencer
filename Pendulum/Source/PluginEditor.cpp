#include "PluginEditor.h"

namespace dy::pendulum {

using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

// ------------------------------------------------------------------ small controls
Knob::Knob (APVTS& s, const juce::String& id, juce::String cap) : caption (std::move (cap))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
}
void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (14)); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.5f));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

Combo::Combo (APVTS& s, const juce::String& id, juce::String cap) : caption (std::move (cap))
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
}
void Combo::resized() { box.setBounds (getLocalBounds().withTrimmedTop (15)); }
void Combo::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (bodyFont (11.0f));
    g.drawText (caption, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

Toggle::Toggle (APVTS& s, const juce::String& id, juce::String text)
{
    setButtonText (text);
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

// ------------------------------------------------------------------ the wave
void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::plate);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    const auto& e = proc.display;
    const int n = e.count();
    auto area = r.reduced (60.0f, 22.0f);
    const float cx = area.getCentreX(), amp = area.getWidth() * 0.5f - 14.0f;
    const float rowH = area.getHeight() / static_cast<float> (n);

    // the line every bob passes through, and the two ends of the swing
    g.setColour (Colours::line.withAlpha (0.9f));
    g.drawVerticalLine (juce::roundToInt (cx), area.getY() - 8.0f, area.getBottom() + 8.0f);
    g.setColour (Colours::line.withAlpha (0.35f));
    g.drawVerticalLine (juce::roundToInt (cx - amp), area.getY(), area.getBottom());
    g.drawVerticalLine (juce::roundToInt (cx + amp), area.getY(), area.getBottom());

    // the snake through all bobs
    juce::Path snake;
    for (int k = 0; k < n; ++k)
    {
        const float y = area.getY() + (k + 0.5f) * rowH, x = cx + static_cast<float> (e.position (k, ppq)) * amp;
        if (k == 0) snake.startNewSubPath (x, y); else snake.lineTo (x, y);
    }
    g.setColour (Colours::brass.withAlpha (0.25f));
    g.strokePath (snake, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));

    for (int k = 0; k < n; ++k)
    {
        const float y = area.getY() + (k + 0.5f) * rowH;
        const float t = n > 1 ? static_cast<float> (k) / static_cast<float> (n - 1) : 0.0f;
        static const juce::Colour layerCols[4] = { Colours::brass, Colours::teal, Colours::lilac, juce::Colour (0xffe58f8f) };
        const auto col = e.getParams().layers > 1 ? layerCols[e.layerOf (k)] : Colours::brass.interpolatedWith (Colours::teal, t);
        g.setColour (Colours::line.withAlpha (0.25f));
        g.drawHorizontalLine (juce::roundToInt (y), cx - amp, cx + amp);

        // a short trail behind the bob
        for (int s = 6; s >= 1; --s)
        {
            const float tx = cx + static_cast<float> (e.position (k, ppq - s * 0.035)) * amp;
            g.setColour (col.withAlpha (0.05f * (7 - s)));
            g.fillEllipse (tx - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
        const float x = cx + static_cast<float> (e.position (k, ppq)) * amp;
        const double age = ppq - e.lastEventBefore (k, ppq);
        // a flash on each note, short enough to stay near the middle line (a third of a half swing)
        const double flash = std::min (0.4, e.getParams().cycleBeats / (2.0 * e.swingsOf (k)) * 0.33);
        const float glow = age >= 0.0 && age < flash ? static_cast<float> (1.0 - age / flash) : 0.0f;
        const float rad = juce::jlimit (3.0f, 9.0f, rowH * 0.32f);
        if (glow > 0.0f)
        {
            g.setColour (col.withAlpha (0.35f * glow));
            g.fillEllipse (x - rad * 2.6f, y - rad * 2.6f, rad * 5.2f, rad * 5.2f);
        }
        g.setColour (col.brighter (0.4f * glow));
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);

        g.setColour (Colours::dim);
        g.setFont (monoFont (10.0f));
        juce::String label = juce::MidiMessage::getMidiNoteName (e.noteOf (k), true, true, 3);
        if (e.getParams().layerChannels) label << " ch" << e.channelOf (k);
        g.drawText (label, juce::Rectangle<float> (r.getRight() - 58.0f, y - 7.0f, 50.0f, 14.0f),
                    juce::Justification::centredRight);
        g.drawText (juce::String (static_cast<int> (e.swingsOf (k))), juce::Rectangle<float> (r.getX() + 8.0f, y - 7.0f, 40.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
    g.setColour (Colours::dim.withAlpha (0.7f));
    g.setFont (displayFont (9.0f));
    g.drawText ("SWINGS", juce::Rectangle<float> (r.getX() + 8.0f, r.getY() + 4.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("NOTE", juce::Rectangle<float> (r.getRight() - 52.0f, r.getY() + 4.0f, 44.0f, 12.0f), juce::Justification::centredRight);
}

void CycleBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    const auto& e = proc.display;
    const double G = e.getParams().cycleBeats, pos = std::fmod (std::fmod (ppq, G) + G, G);
    const int bars = static_cast<int> (std::lround (G / 4.0));
    auto bar = r.reduced (12.0f, 0.0f).withTrimmedTop (20.0f).withHeight (8.0f);
    g.setColour (Colours::scopeBg);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (Colours::brass);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * static_cast<float> (pos / G)), 4.0f);
    g.setColour (Colours::line);
    for (int b = 1; b < bars; ++b)
        g.drawVerticalLine (juce::roundToInt (bar.getX() + bar.getWidth() * b / bars), bar.getY() - 2.0f, bar.getBottom() + 2.0f);

    const double coh = e.coherence (ppq);
    g.setFont (monoFont (11.0f));
    g.setColour (Colours::dim);
    const int cycleNo = static_cast<int> (std::floor (ppq / G)) + 1;
    g.drawText ("cycle " + juce::String (cycleNo) + "   bar " + juce::String (static_cast<int> (pos / 4.0) + 1) + " of " + juce::String (bars),
                r.reduced (12.0f, 0.0f).removeFromTop (18.0f), juce::Justification::centredLeft);
    g.setColour (coh > 0.8 ? Colours::brass : Colours::dim);
    g.drawText ("in phase " + juce::String (juce::roundToInt (coh * 100.0)) + " %", r.reduced (12.0f, 0.0f).removeFromTop (18.0f), juce::Justification::centredRight);
}

// ------------------------------------------------------------------ editor
PendulumEditor::PendulumEditor (PendulumProcessor& p)
    : AudioProcessorEditor (p), proc (p), wave (p), cycle (p),
      count (p.apvts, PID::count, "Pendulums"), swings (p.apvts, PID::swings, "Swings"), root (p.apvts, PID::root, "Root"),
      step (p.apvts, PID::step, "Step"), length (p.apvts, PID::length, "Length"), velocity (p.apvts, PID::velocity, "Velocity"),
      accent (p.apvts, PID::accent, "Phase accent"), level (p.apvts, PID::previewLevel, "Level"),
      cycleBars (p.apvts, PID::cycleBars, "Cycle"), trigger (p.apvts, PID::trigger, "Notes on"), quantize (p.apvts, PID::quantize, "Quantize"),
      scale (p.apvts, PID::scale, "Scale"), channel (p.apvts, PID::channel, "MIDI channel"), run (p.apvts, PID::run, "Run"),
      noteMode (p.apvts, PID::noteMode, "Notes"), layers (p.apvts, PID::layers, "Layers"),
      descending (p.apvts, PID::descending, "Long = high"), preview (p.apvts, PID::preview, "Preview sound"),
      layerChannels (p.apvts, PID::layerChannels, "Layer = channel")
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &wave, &cycle, &count, &swings, &root, &step, &length, &velocity, &accent, &level,
                                                              &cycleBars, &trigger, &quantize, &scale, &channel, &run, &descending, &preview,
                                                              &noteMode, &layers, &layerChannels })
        content.addAndMakeVisible (c);
    swings.slider.setTooltip ("Full swings the longest pendulum makes in one cycle; each next one makes one more");
    accent.slider.setTooltip ("Notes get louder the more the pendulums are in phase: loudest when they line up");
    layers.box.setTooltip ("Split the pendulums into groups, each two octaves higher than the last: on each receiving track, "
                           "Live's Pitch device (Lowest / Range, Block) picks which group that track plays");
    noteMode.box.setTooltip ("Drum pads: pendulum 1 plays C1, the next C#1 and so on, one Drum Rack pad each");
    layerChannels.setTooltip ("Each layer also on its own MIDI channel, for multitimbral instruments and hardware");
    preview.setTooltip ("A built-in bell so you hear it straight away. Turn off to play another instrument from the MIDI output");
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kW) / kH);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    startTimerHz (60);
}

PendulumEditor::~PendulumEditor() { stopTimer(); setLookAndFeel (nullptr); }

void PendulumEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::frame);
    const float s = getWidth() / static_cast<float> (kW);
    g.addTransform (juce::AffineTransform::scale (s));
    g.setColour (Colours::line);
    g.fillRect (0, 46, kW, 1);
    g.setFont (displayFont (19.0f));
    g.setColour (Colours::text);
    g.drawText ("DY", 16, 0, 40, 46, juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.drawText ("PENDULUM", 48, 0, 160, 46, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (monoFont (11.0f));
    g.drawText ("v" DY_VERSION_STRING "  pendulum wave MIDI", 178, 0, 300, 46, juce::Justification::centredLeft);
    const bool synced = proc.telemetry.hostSynced.load(), running = proc.telemetry.running.load();
    g.setColour (synced ? Colours::teal : Colours::dim);
    g.drawText (juce::String (proc.telemetry.bpm.load(), 1) + " bpm   " + (synced ? "synced to the song" : running ? "free running" : "waiting for play"),
                kW - 420, 0, 404, 46, juce::Justification::centredRight);

    const char* titles[4] = { "WAVE", "NOTES", "PLAY", "OUTPUT" };
    for (int i = 0; i < 4; ++i)
    {
        auto r = panels[i].toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (titles[i], panels[i].getX() + 12, panels[i].getY() + 8, 200, 14, juce::Justification::centredLeft);
    }
}

void PendulumEditor::resized()
{
    const float s = getWidth() / static_cast<float> (kW);
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    const int pad = 12, top = 58;
    wave.setBounds (pad, top, 700, kH - top - pad - 58);
    cycle.setBounds (pad, wave.getBottom() + 10, 700, 48);

    const int x0 = wave.getRight() + pad, w = kW - x0 - pad;
    int y = top;
    const int heights[4] = { 128, 176, 128, 220 };
    for (int i = 0; i < 4; ++i) { panels[i] = { x0, y, w, heights[i] }; y += heights[i] + 10; }
    auto inner = [&] (int i) { auto r = panels[i].reduced (10, 8); r.removeFromTop (20); return r; };
    {
        auto r = inner (0);
        const int kw = r.getWidth() / 3;
        count.setBounds (r.removeFromLeft (kw).withHeight (92));
        swings.setBounds (r.removeFromLeft (kw).withHeight (92));
        cycleBars.setBounds (r.withHeight (40).withY (r.getY() + 20));
    }
    {
        auto r = inner (1);
        auto row = r.removeFromTop (92);
        const int kw = row.getWidth() / 3;
        root.setBounds (row.removeFromLeft (kw));
        step.setBounds (row.removeFromLeft (kw));
        descending.setBounds (row.withSizeKeepingCentre (row.getWidth() - 8, 26));
        r.removeFromTop (6);
        auto row2 = r.removeFromTop (40);
        noteMode.setBounds (row2.removeFromRight (110));
        row2.removeFromRight (8);
        scale.setBounds (row2);
    }
    {
        auto r = inner (2);
        const int kw = r.getWidth() / 3;
        length.setBounds (r.removeFromLeft (kw).withHeight (92));
        velocity.setBounds (r.removeFromLeft (kw).withHeight (92));
        accent.setBounds (r.withHeight (92));
    }
    {
        auto r = inner (3);
        auto row = r.removeFromTop (40);
        const int cw = (row.getWidth() - 8) / 2;
        trigger.setBounds (row.removeFromLeft (cw));
        row.removeFromLeft (8);
        quantize.setBounds (row);
        r.removeFromTop (6);
        auto row2 = r.removeFromTop (40);
        layers.setBounds (row2.removeFromLeft (cw));
        row2.removeFromLeft (8);
        channel.setBounds (row2);
        r.removeFromTop (6);
        auto rowL = r.removeFromTop (40);
        run.setBounds (rowL.removeFromLeft (cw));
        rowL.removeFromLeft (8);
        layerChannels.setBounds (rowL.withTrimmedTop (14));
        r.removeFromTop (8);
        auto row3 = r.removeFromTop (26);
        preview.setBounds (row3.removeFromLeft (cw));
        row3.removeFromLeft (8);
        level.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        level.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        level.setBounds (row3.withY (row3.getY() - 14).withHeight (40));
    }
}

void PendulumEditor::timerCallback()
{
    // the drawing copy of the settings, and a smooth position between audio blocks
    proc.display.setParams (proc.readParams());
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double tp = proc.telemetry.ppq.load();
    if (tp != lastTelemetryPpq) { lastTelemetryPpq = tp; lastUpdate = now; shownPpq = tp; }
    else if (proc.telemetry.running.load()) shownPpq = tp + (now - lastUpdate) * proc.telemetry.bpm.load() / 60.0;
    // the display engine only knows when notes were hit if it sees them: mirror them from the timing
    wave.ppq = cycle.ppq = shownPpq;
    wave.repaint();
    cycle.repaint();
    static int t = 0;
    if (++t % 30 == 0) repaint (0, 0, getWidth(), juce::roundToInt (46 * getWidth() / static_cast<float> (kW)));
}

} // namespace dy::pendulum
