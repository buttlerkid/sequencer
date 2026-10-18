#include "StepStrips.h"

namespace dy::dub {

// ------------------------------------------------------------------ strip
StepStrip::StepStrip (StripArea& owner, DubProcessor& p, int i) : index (i), area (owner), proc (p)
{
    setupKnob (cut, StepStore::Cut, 1.0, [] (double v)
    {
        const double hz = cutToHz (v * 10.0);
        return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + "k" : juce::String (juce::roundToInt (hz));
    });
    setupKnob (res, StepStore::Res, 0.0, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; });
    setupKnob (vol, StepStore::Vol, 1.0, [] (double v)
    {
        return v < 0.001 ? juce::String ("-inf") : juce::String (20.0 * std::log10 (v), 1) + "dB";
    });
    cut.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    res.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    vol.setSliderStyle (juce::Slider::LinearVertical);
    cut.setTooltip ("Cutoff of step " + juce::String (i + 1) + " (double-click: open)");
    res.setTooltip ("Resonance of step " + juce::String (i + 1));
    vol.setTooltip ("Level of step " + juce::String (i + 1) + " (double-click: 0 dB)");
    addAndMakeVisible (onKey);
    sync();
}

void StepStrip::setupKnob (juce::Slider& s, int field, double def, std::function<juce::String (double)> text)
{
    s.setRange (0.0, 1.0);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity (160);
    s.setDoubleClickReturnValue (true, def);
    s.textFromValueFunction = std::move (text);
    s.onValueChange = [this, &s, field]
    {
        if (! syncing) proc.steps.set (area.getBank(), index, field, static_cast<float> (s.getValue()));
        repaint();
    };
    s.addMouseListener (this, false);
    addAndMakeVisible (s);
}

void StepStrip::sync()
{
    syncing = true;
    const int b = area.getBank();
    if (! cut.isMouseButtonDown()) cut.setValue (proc.steps.get (b, index, StepStore::Cut), juce::dontSendNotification);
    if (! res.isMouseButtonDown()) res.setValue (proc.steps.get (b, index, StepStore::Res), juce::dontSendNotification);
    if (! vol.isMouseButtonDown()) vol.setValue (proc.steps.get (b, index, StepStore::Vol), juce::dontSendNotification);
    syncing = false;
    // off steps step back, so the ones that play stand out
    const float a = proc.steps.get (b, index, StepStore::On) > 0.5f ? 1.0f : 0.5f;
    for (auto* s : { &cut, &res, &vol }) s->setAlpha (a);
    onKey.repaint();
}

void StepStrip::setState (bool sel, bool play, bool follow, bool inLen, float m)
{
    const bool changed = sel != selected || play != playing || follow != follower || inLen != inLength;
    const bool meterChanged = std::abs (m - meter) > 0.004f;
    selected = sel; playing = play; follower = follow; inLength = inLen; meter = m;
    if (changed)
    {
        for (auto* s : { &cut, &res, &vol }) s->setEnabled (! follower);
        setAlpha (inLength ? 1.0f : 0.45f);
        repaint();
    }
    else if (meterChanged) repaint (rMeter.expanded (2));
}

void StepStrip::resized()
{
    auto r = getLocalBounds().reduced (4, 0);
    r.removeFromTop (6);
    rNum = r.removeFromTop (20);
    r.removeFromTop (4);
    rCutCap = r.removeFromTop (13);
    cut.setBounds (r.removeFromTop (64));
    r.removeFromTop (4);
    rResCap = r.removeFromTop (13);
    res.setBounds (r.removeFromTop (64));
    r.removeFromTop (6);
    rVolCap = r.removeFromTop (13);
    auto onArea = r.removeFromBottom (34).reduced (2, 4);
    onKey.setBounds (onArea);
    r.removeFromBottom (2);
    auto faderArea = r;
    const int fw = 30;
    vol.setBounds (faderArea.getCentreX() - fw / 2 - 5, faderArea.getY(), fw, faderArea.getHeight());
    rMeter = { vol.getRight() + 5, faderArea.getY() + 6, 4, faderArea.getHeight() - 12 };
}

juce::String StepStrip::caption (juce::Slider& s, const char* name) const
{
    if (s.isMouseOverOrDragging() && s.isEnabled()) return s.getTextFromValue (s.getValue());
    return name;
}

void StepStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    drawRaised (g, r, 5.0f);
    if (follower)
    {
        g.setColour (Colours::frame.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 5.0f);
    }

    // playhead LED and the step number
    auto led = juce::Rectangle<float> (r.getCentreX() - 9.0f, 4.0f, 18.0f, 3.0f);
    g.setColour (playing ? Colours::brass : Colours::edge);
    g.fillRoundedRectangle (led, 1.5f);
    if (playing)
    {
        g.setColour (Colours::brass.withAlpha (0.25f));
        g.fillRoundedRectangle (led.expanded (3.0f, 2.0f), 3.0f);
    }
    g.setFont (monoFont (11.5f));
    g.setColour (playing ? Colours::brass : selected ? Colours::bright : Colours::dim);
    g.drawText (juce::String (index + 1).paddedLeft ('0', 2), rNum, juce::Justification::centred);

    g.setFont (displayFont (9.0f));
    auto cap = [&] (juce::Rectangle<int> a, juce::Slider& s, const char* name)
    {
        const bool v = s.isMouseOverOrDragging() && s.isEnabled();
        g.setColour (v ? Colours::brass : Colours::dim);
        if (v) g.setFont (monoFont (10.5f)); else g.setFont (displayFont (9.0f));
        g.drawText (caption (s, name), a, juce::Justification::centred);
    };
    cap (rCutCap, cut, "CUT");
    cap (rResCap, res, "RES");
    cap (rVolCap, vol, "VOL");

    // the level meter beside the fader: lit while this step plays
    auto m = rMeter.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (m, 2.0f);
    if (meter > 0.002f)
    {
        const float h = m.getHeight() * juce::jlimit (0.0f, 1.0f, meter);
        juce::ColourGradient lit (Colours::brass, m.getX(), m.getBottom(), juce::Colour (0xfff2dc8a), m.getX(), m.getY(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (m.withTop (m.getBottom() - h).reduced (0.8f, 0.0f), 1.5f);
    }

    if (selected)
    {
        g.setColour (Colours::bright.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.75f), 5.0f, 1.5f);
    }
}

void StepStrip::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == this) area.select (index);
}

void StepStrip::OnKey::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool on = strip.proc.steps.get (strip.area.getBank(), strip.index, StepStore::On) > 0.5f;
    drawRaised (g, r, 4.0f, isMouseOver());
    if (on)
    {
        g.setColour (Colours::brass.withAlpha (0.10f));
        g.fillRoundedRectangle (r.reduced (1.0f), 3.5f);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (r.reduced (1.0f), 3.5f);
    }
    g.setFont (displayFont (11.0f));
    g.setColour (on ? Colours::brass : Colours::dim.withAlpha (0.8f));
    g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
}

void StepStrip::OnKey::mouseDown (const juce::MouseEvent&)
{
    auto& st = strip.proc.steps;
    const int b = strip.area.getBank();
    st.set (b, strip.index, StepStore::On, st.get (b, strip.index, StepStore::On) > 0.5f ? 0.0f : 1.0f);
    repaint();
}

// ------------------------------------------------------------------ links
void LinkButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const auto c = ! enabled ? Colours::edge : on ? Colours::brass : Colours::dim.withAlpha (0.6f);
    const float w = r.getWidth() * 0.58f;
    g.setColour (c);
    g.drawRoundedRectangle (r.withWidth (w), r.getHeight() * 0.5f, 1.6f);
    g.drawRoundedRectangle (r.withTrimmedLeft (r.getWidth() - w), r.getHeight() * 0.5f, 1.6f);
    if (on)
    {
        g.setColour (Colours::brass.withAlpha (0.2f));
        g.fillRoundedRectangle (r.expanded (2.0f, 1.0f), 4.0f);
    }
}

void LinkButton::mouseDown (const juce::MouseEvent&)
{
    if (! enabled) return;
    auto& st = area.proc.steps;
    st.set (area.bank, leftStep, StepStore::Chain, on ? 0.0f : 1.0f);
    area.tick();
}

// ------------------------------------------------------------------ area
StripArea::StripArea (DubProcessor& p) : proc (p)
{
    for (int s = 0; s < kSteps; ++s)
    {
        strips.push_back (std::make_unique<StepStrip> (*this, p, s));
        addAndMakeVisible (*strips.back());
    }
    for (int s = 0; s < kSteps - 1; ++s)
    {
        links.push_back (std::make_unique<LinkButton> (*this, s));
        addAndMakeVisible (*links.back());
    }
}

void StripArea::setBank (int b)
{
    bank = b;
    shownVersion = -1;
    tick();
    repaint();
}

void StripArea::select (int step)
{
    selected = step;
    if (onSelect) onSelect (step);
    tick();
}

void StripArea::resized()
{
    const float w = (getWidth() - kGutter) / static_cast<float> (kSteps);
    for (int s = 0; s < kSteps; ++s)
        strips[static_cast<size_t> (s)]->setBounds (juce::roundToInt (kGutter + s * w), 14, juce::roundToInt (w), getHeight() - 14);
    for (int s = 0; s < kSteps - 1; ++s)
        links[static_cast<size_t> (s)]->setBounds (juce::roundToInt (kGutter + (s + 1) * w) - 11, 1, 22, 13);
}

void StripArea::paint (juce::Graphics& g)
{
    // the gutter: which bank is on view, and which one plays
    auto gut = juce::Rectangle<float> (0.0f, 14.0f, kGutter - 6.0f, static_cast<float> (getHeight()) - 14.0f);
    drawInset (g, gut, 6.0f);
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.0f));
    g.drawText ("BANK", gut.withHeight (34.0f), juce::Justification::centred);
    g.setColour (Colours::brass);
    g.setFont (displayFont (34.0f));
    g.drawText (bank == 0 ? "A" : "B", gut.withTrimmedTop (24.0f).withHeight (44.0f), juce::Justification::centred);

    const auto& tel = proc.telemetry();
    if (tel.running.load() && tel.step.load() >= 0)
    {
        g.setColour (Colours::dim);
        g.setFont (displayFont (8.5f));
        g.drawText ("PLAYING", gut.withTrimmedTop (80.0f).withHeight (14.0f), juce::Justification::centred);
        g.setColour (tel.bank.load() == bank ? Colours::brass : Colours::text);
        g.setFont (displayFont (16.0f));
        g.drawText (tel.bank.load() == 0 ? "A" : "B", gut.withTrimmedTop (94.0f).withHeight (20.0f), juce::Justification::centred);
    }
}

void StripArea::paintOverChildren (juce::Graphics& g)
{
    // chained blocks: a bar across the tops of the joined strips
    const int len = static_cast<int> (proc.apvts.getRawParameterValue (PID::length)->load());
    for (int s = 0; s < kSteps - 1; ++s)
    {
        if (! links[static_cast<size_t> (s)]->on) continue;
        auto a = strips[static_cast<size_t> (s)]->getBounds(), b = strips[static_cast<size_t> (s + 1)]->getBounds();
        g.setColour (Colours::brass.withAlpha (0.75f));
        g.fillRect (juce::Rectangle<float> (static_cast<float> (a.getCentreX()), static_cast<float> (a.getY()) + 1.0f,
                                            static_cast<float> (b.getCentreX() - a.getCentreX()), 2.0f));
        juce::ignoreUnused (len);
    }
}

void StripArea::tick()
{
    const auto& tel = proc.telemetry();
    const int v = proc.steps.version.load();
    const bool resync = v != shownVersion;
    shownVersion = v;
    const int len = static_cast<int> (proc.apvts.getRawParameterValue (PID::length)->load());
    const bool running = tel.running.load() && tel.step.load() >= 0;
    const int pb = running ? tel.bank.load() : -1, ps = tel.step.load(), pBlock = tel.blockStart.load();

    int blockOf[kSteps];
    for (int s = 0; s < kSteps; ++s)
        blockOf[s] = (s > 0 && s < len && proc.steps.get (bank, s - 1, StepStore::Chain) > 0.5f) ? blockOf[s - 1] : s;

    meterHold = std::max (tel.stepGain.load(), meterHold * 0.82f);
    for (int s = 0; s < kSteps; ++s)
    {
        auto& st = *strips[static_cast<size_t> (s)];
        if (resync) st.sync();
        const bool playing = pb == bank && s == ps;
        const bool inPlayingBlock = pb == bank && blockOf[s] == pBlock;
        st.setState (blockOf[s] == blockOf[clampT (selected, 0, kSteps - 1)], playing, blockOf[s] != s, s < len,
                     inPlayingBlock && s == pBlock ? meterHold : 0.0f);
    }
    for (int s = 0; s < kSteps - 1; ++s)
    {
        auto& l = *links[static_cast<size_t> (s)];
        const bool on = proc.steps.get (bank, s, StepStore::Chain) > 0.5f && s < len - 1;
        const bool en = s < len - 1;
        if (on != l.on || en != l.enabled) { l.on = on; l.enabled = en; l.repaint(); repaint(); }
    }
    if (pb != playBank) { playBank = pb; repaint (0, 0, kGutter, getHeight()); }
}

} // namespace dy::dub
