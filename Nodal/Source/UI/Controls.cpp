#include "Controls.h"
#include "NodalLookAndFeel.h"
#include "Engine/Tuning.h"

namespace dy::nodal {

// ------------------------------------------------------------------ Panel
void Panel::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
    if (caption.isNotEmpty())
    {
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (caption.toUpperCase(), 12, 8, getWidth() - 24, 14, juce::Justification::centredLeft);
    }
}

// ------------------------------------------------------------------ Knob
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& cap, bool isBig)
    : caption (cap), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    slider.setMouseDragSensitivity (big ? 260 : 180);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + " - drag up/down, double-click to reset, click the value to type");
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (big ? 0 : 14);
    slider.setBounds (r);
}

void Knob::paint (juce::Graphics& g)
{
    if (big) return;
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.5f));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

// ------------------------------------------------------------------ ChoiceButtons
ChoiceButtons::ChoiceButtons (APVTS& state, const juce::String& paramId, std::vector<int> shown, IconFn iconFn)
    : param (*state.getParameter (paramId)), icon (std::move (iconFn)),
      attachment (*state.getParameter (paramId), [this] (float v) { current = juce::roundToInt (v); repaint(); if (onChange) onChange (current); },
                  state.undoManager)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param)) labels = c->choices;
    if (shown.empty()) for (int i = 0; i < labels.size(); ++i) shown.push_back (i);
    choices = std::move (shown);
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> ChoiceButtons::cell (int i) const
{
    const float w = getWidth() / static_cast<float> (choices.size());
    return { i * w, 0.0f, w, static_cast<float> (getHeight()) };
}

int ChoiceButtons::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < static_cast<int> (choices.size()); ++i) if (cell (i).contains (p)) return i;
    return -1;
}

void ChoiceButtons::resized() {}

void ChoiceButtons::paint (juce::Graphics& g)
{
    const bool gaps = icon != nullptr;
    for (int i = 0; i < static_cast<int> (choices.size()); ++i)
    {
        auto r = cell (i);
        if (gaps) r = r.reduced (2.5f, 0.0f);
        const int choice = choices[static_cast<size_t> (i)];
        const bool on = choice == current;

        if (gaps)
        {
            g.setColour (i == hover ? Colours::panel2.brighter (0.08f) : Colours::panel2);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (on ? Colours::brass : Colours::line);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.4f : 1.0f);
            auto iconArea = r.withTrimmedBottom (16.0f).reduced (6.0f, 5.0f);
            icon (g, iconArea, choice, on);
            g.setColour (on ? Colours::brass : Colours::text);
            g.setFont (bodyFont (10.5f, true));
            g.drawFittedText (labels[choice], r.removeFromBottom (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
        else
        {
            g.setColour (on ? Colours::brass : (i == hover ? Colours::panel2.brighter (0.08f) : Colours::panel2));
            g.fillRect (r);
            if (i > 0) { g.setColour (Colours::line); g.drawVerticalLine (juce::roundToInt (r.getX()), r.getY(), r.getBottom()); }
            g.setColour (on ? Colours::ink : Colours::text);
            g.setFont (bodyFont (12.0f, true));
            g.drawFittedText (labels[choice], r.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.75f);
        }
    }
    if (! gaps)
    {
        g.setColour (Colours::line);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
    }
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0) attachment.setValueAsCompleteGesture (static_cast<float> (choices[static_cast<size_t> (i)]));
}

void ChoiceButtons::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover) { hover = i; repaint(); }
}

// ------------------------------------------------------------------ ParamToggle
ParamToggle::ParamToggle (APVTS& state, const juce::String& paramId, juce::String onText, juce::String offText)
    : on (std::move (onText)), off (std::move (offText))
{
    setClickingTogglesState (true);
    onStateChange = [this] { setButtonText (getToggleState() ? on : off); };
    att = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    setButtonText (getToggleState() ? on : off);
}

// ------------------------------------------------------------------ ParamCombo
ParamCombo::ParamCombo (APVTS& state, const juce::String& paramId, const juce::String& cap) : caption (cap)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
}

void ParamCombo::resized() { box.setBounds (getLocalBounds().withTrimmedTop (15)); }

void ParamCombo::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (bodyFont (11.0f));
    g.drawText (caption, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ Meters
void Meters::tick()
{
    auto conv = [] (float peak) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 60.0f) / 60.0f); };
    const float ni = conv (proc.telemetry.peakIn.load()), no = conv (proc.telemetry.peakOut.load());
    const float nin = std::max (ni, in - 0.02f), nout = std::max (no, out - 0.02f);
    if (std::abs (nin - in) + std::abs (nout - out) > 0.002f) { in = nin; out = nout; repaint(); }
}

void Meters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rowH = r.getHeight() / 2.0f;
    auto bar = [&] (const juce::String& name, float v, juce::Rectangle<float> row)
    {
        g.setColour (Colours::dim);
        g.setFont (monoFont (10.0f));
        g.drawText (name, row.removeFromLeft (30.0f), juce::Justification::centredLeft);
        auto b = row.withSizeKeepingCentre (row.getWidth(), 8.0f);
        g.setColour (juce::Colour (0xff0e1218));
        g.fillRoundedRectangle (b, 3.0f);
        juce::ColourGradient grad (juce::Colour (0xff4f8f9b), b.getX(), 0, Colours::brass, b.getRight(), 0, false);
        grad.addColour (0.6, Colours::teal);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * v), 3.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
    };
    bar ("IN", in, r.removeFromTop (rowH));
    bar ("OUT", out, r);
}

// ------------------------------------------------------------------ Spectrum
bool Spectrum::tick()
{
    const int active = proc.telemetry.active.load();
    bool changed = false;
    for (int i = 0; i < kMaxModes; ++i)
    {
        const size_t k = static_cast<size_t> (i);
        const float e = i < active ? proc.telemetry.energy[k].load() : 0.0f;
        if (std::abs (e - shown[k]) > 1e-7f || smooth[k] > 1e-6f) changed = true;
        shown[k] = e;
    }
    const int key = static_cast<int> (std::lround (proc.params.key->load())), scale = static_cast<int> (std::lround (proc.params.scale->load()));
    if (key != shownKey || scale != shownScale) { shownKey = key; shownScale = scale; changed = true; }
    if (changed) repaint();
    return changed;
}

void Spectrum::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0e1218));
    g.fillRoundedRectangle (r, 6.0f);

    const int key   = static_cast<int> (std::lround (proc.params.key->load()));
    const int scale = static_cast<int> (std::lround (proc.params.scale->load()));
    const auto& sc  = kScales[juce::jlimit (0, kNumScales - 1, scale)];
    const double lo = 24.0, hi = 108.0;
    auto xOf = [&] (double note) { return r.getX() + static_cast<float> ((note - lo) / (hi - lo)) * r.getWidth(); };
    const float keyW = r.getWidth() / static_cast<float> (hi - lo);

    for (int n = static_cast<int> (lo); n <= static_cast<int> (hi); ++n)
    {
        const double off = (((n - key) % 12) + 12) % 12 * 100.0;
        bool inScale = false;
        for (int i = 0; i < sc.size; ++i) if (std::abs (sc.cents[i] - off) < 30.0) inScale = true;
        g.setColour (inScale ? (off == 0 ? Colours::brass.withAlpha (0.30f) : Colours::text.withAlpha (0.10f)) : Colours::text.withAlpha (0.025f));
        g.fillRect (xOf (n) - keyW * 0.42f, r.getY() + 2.0f, keyW * 0.84f, r.getHeight() - 4.0f);
    }

    const int active = proc.telemetry.active.load(), dom = proc.telemetry.dominant.load();
    float mx = 1e-9f;
    for (int i = 0; i < active; ++i)
    {
        smooth[static_cast<size_t> (i)] = std::max (proc.telemetry.energy[static_cast<size_t> (i)].load(), smooth[static_cast<size_t> (i)] * 0.88f);
        if (smooth[static_cast<size_t> (i)] < 1e-7f) smooth[static_cast<size_t> (i)] = 0.0f;
        mx = std::max (mx, smooth[static_cast<size_t> (i)]);
    }
    for (int i = 0; i < active; ++i)
    {
        const double f = proc.telemetry.freq[static_cast<size_t> (i)].load();
        if (f <= 0) continue;
        const double note = hzToNote (f);
        if (note < lo || note > hi) continue;
        const float e = std::sqrt (smooth[static_cast<size_t> (i)] / mx);
        const float h = 5.0f + e * (r.getHeight() - 12.0f);
        g.setColour (i == dom && e > 0.05f ? Colours::brass : Colours::teal.withAlpha (0.85f));
        g.fillRect (xOf (note) - 1.0f, r.getBottom() - 3.0f - h, 2.0f, h);
    }

    g.setColour (Colours::dim);
    g.setFont (monoFont (9.5f));
    for (int n = 36; n <= static_cast<int> (hi); n += 12)
        g.drawText ("C" + juce::String (n / 12 - 1), juce::Rectangle<float> (xOf (n) + 2.0f, r.getY() + 3.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace dy::nodal
