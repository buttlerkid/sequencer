#include "DubLookAndFeel.h"

namespace dy::dub {

void drawInset (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (Colours::inset);
    g.fillRoundedRectangle (r, corner);
    // a darker top edge and a faint light bottom edge: pressed into the panel
    g.setColour (Colours::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawHorizontalLine (juce::roundToInt (r.getBottom()) , r.getX() + corner, r.getRight() - corner);
}

void drawRaised (juce::Graphics& g, juce::Rectangle<float> r, float corner, bool lit)
{
    juce::ColourGradient body (Colours::panel2.brighter (lit ? 0.12f : 0.04f), r.getX(), r.getY(),
                               Colours::panel.darker (0.12f), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (lit ? 0.14f : 0.07f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.5f), r.getX() + corner, r.getRight() - corner);
}

void drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, float a0, float a1, bool bipolar, float zeroPos,
               bool enabled, const juce::NamedValueSet* props)
{
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto sq = bounds.withSizeKeepingCentre (size, size);
    const auto c = sq.getCentre();
    const float R = size * 0.5f - 1.0f;
    const float arcW = juce::jmax (2.2f, R * 0.085f);
    const float arcR = R - arcW * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const float alpha = enabled ? 1.0f : 0.4f;

    // the value arc in its groove
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (Colours::edge.withMultipliedAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float zero = bipolar ? a0 + zeroPos * (a1 - a0) : a0;
    if (std::abs (ang - zero) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (zero, ang), juce::jmax (zero, ang), true);
        g.setColour ((enabled ? Colours::brass : Colours::brassDim).withMultipliedAlpha (enabled ? 1.0f : 0.7f));
        g.strokePath (v, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // modulation: a thin ring outside, from the set value to where the source has taken it
    if (props != nullptr && props->contains ("modPos"))
    {
        const float mp = juce::jlimit (0.0f, 1.0f, static_cast<float> (static_cast<double> ((*props)["modPos"])));
        const float ma = a0 + mp * (a1 - a0);
        const juce::Colour mc (static_cast<juce::uint32> (static_cast<juce::int64> ((*props)["modColour"])));
        const float ringR = arcR - arcW * 1.25f;
        juce::Path m;
        m.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, juce::jmin (ma, ang), juce::jmax (ma, ang), true);
        g.setColour (mc.withAlpha (0.95f));
        g.strokePath (m, juce::PathStrokeType (juce::jmax (1.5f, arcW * 0.6f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> dot (c.x + ringR * std::sin (ma), c.y - ringR * std::cos (ma));
        g.fillEllipse (dot.x - 2.0f, dot.y - 2.0f, 4.0f, 4.0f);
    }

    // the knob: shadow, body, rim, cap, highlight
    const float kr = R - arcW * 2.6f;
    if (kr < 4.0f) return;
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.16f * alpha));
        const float e = i * 1.6f;
        g.fillEllipse (c.x - kr - e * 0.5f, c.y - kr + e * 0.9f - e * 0.5f, (kr + e * 0.5f) * 2.0f, (kr + e * 0.5f) * 2.0f);
    }
    juce::ColourGradient body (juce::Colour (0xff4b4e53).withMultipliedAlpha (alpha), c.x - kr * 0.6f, c.y - kr * 0.8f,
                               juce::Colour (0xff0d0e0f).withMultipliedAlpha (alpha), c.x + kr * 0.5f, c.y + kr, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    juce::ColourGradient rim (juce::Colour (0xff6a6d72).withMultipliedAlpha (alpha), c.x, c.y - kr,
                              juce::Colour (0xff050506).withMultipliedAlpha (alpha), c.x, c.y + kr, false);
    g.setGradientFill (rim);
    g.drawEllipse (c.x - kr + 0.5f, c.y - kr + 0.5f, kr * 2.0f - 1.0f, kr * 2.0f - 1.0f, 1.0f);
    const float cr = kr * 0.84f;
    juce::ColourGradient cap (juce::Colour (0xff2b2d30).withMultipliedAlpha (alpha), c.x, c.y - cr,
                              juce::Colour (0xff141516).withMultipliedAlpha (alpha), c.x, c.y + cr, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    juce::ColourGradient spec (juce::Colours::white.withAlpha (0.16f * alpha), c.x - cr * 0.35f, c.y - cr * 0.75f,
                               juce::Colours::white.withAlpha (0.0f), c.x - cr * 0.1f, c.y, true);
    g.setGradientFill (spec);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 1.4f);

    // pointer: a white dot near the edge
    const float pr = cr * 0.68f, ds = juce::jmax (2.2f, kr * 0.15f);
    const juce::Point<float> p (c.x + pr * std::sin (ang), c.y - pr * std::cos (ang));
    g.setColour (Colours::bright.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
    g.fillEllipse (p.x - ds, p.y - ds, ds * 2.0f, ds * 2.0f);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // a dub plate: black vinyl, grooves, an amber label
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    auto d = r.withSizeKeepingCentre (s, s);
    const auto c = d.getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (d.translated (0.0f, 1.5f));
    juce::ColourGradient vinyl (juce::Colour (0xff3a3c40), c.x - s * 0.3f, c.y - s * 0.4f, juce::Colour (0xff070708), c.x + s * 0.3f, c.y + s * 0.5f, true);
    g.setGradientFill (vinyl);
    g.fillEllipse (d);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float k = 0.62f; k < 0.97f; k += 0.08f)
        g.drawEllipse (d.withSizeKeepingCentre (s * k, s * k), 0.7f);
    juce::ColourGradient label (Colours::brass.brighter (0.25f), c.x, c.y - s * 0.2f, Colours::brass.darker (0.45f), c.x, c.y + s * 0.2f, false);
    g.setGradientFill (label);
    g.fillEllipse (d.withSizeKeepingCentre (s * 0.38f, s * 0.38f));
    g.setColour (Colours::ink);
    g.fillEllipse (d.withSizeKeepingCentre (s * 0.07f, s * 0.07f));
}

DubLookAndFeel::DubLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Colours::page);
    setColour (Label::textColourId, Colours::text);
    setColour (Slider::textBoxTextColourId, Colours::brass);
    setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Colours::brass.withAlpha (0.35f));
    setColour (TextButton::buttonColourId, Colours::panel2);
    setColour (TextButton::buttonOnColourId, Colours::panel2);
    setColour (TextButton::textColourOffId, Colours::text);
    setColour (TextButton::textColourOnId, Colours::brass);
    setColour (ComboBox::backgroundColourId, Colours::inset);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::outlineColourId, Colours::edge);
    setColour (ComboBox::arrowColourId, Colours::dim);
    setColour (PopupMenu::backgroundColourId, Colours::frame);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::headerTextColourId, Colours::dim);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours::brass);
    setColour (PopupMenu::highlightedTextColourId, Colours::ink);
    setColour (TooltipWindow::backgroundColourId, Colours::frame);
    setColour (TooltipWindow::textColourId, Colours::text);
    setColour (TooltipWindow::outlineColourId, Colours::line);
    setColour (TextEditor::backgroundColourId, Colours::inset);
    setColour (TextEditor::textColourId, Colours::text);
    setColour (TextEditor::highlightColourId, Colours::brass.withAlpha (0.35f));
    setColour (TextEditor::outlineColourId, Colours::line);
    setColour (TextEditor::focusedOutlineColourId, Colours::brass);
    setColour (CaretComponent::caretColourId, Colours::brass);
    setColour (AlertWindow::backgroundColourId, Colours::frame);
    setColour (AlertWindow::textColourId, Colours::text);
    setColour (AlertWindow::outlineColourId, Colours::line);
    setColour (BubbleComponent::backgroundColourId, Colours::inset);
    setColour (BubbleComponent::outlineColourId, Colours::brass);
}

void DubLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, a0, a1, bipolar,
              bipolar ? static_cast<float> (s.valueToProportionOfLength (0.0)) : 0.0f, s.isEnabled(), &s.getProperties());
}

void DubLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float alpha = s.isEnabled() ? 1.0f : 0.4f;
    if (style == juce::Slider::LinearVertical)
    {
        // fader: a slot with ticks, a metal cap
        const float capH = 16.0f, capW = juce::jmin (r.getWidth() - 2.0f, 26.0f);
        const float top = r.getY() + capH * 0.5f, bottom = r.getBottom() - capH * 0.5f;
        auto slot = juce::Rectangle<float> (r.getCentreX() - 2.5f, top - 3.0f, 5.0f, bottom - top + 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f * alpha));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.05f * alpha));
        g.drawRoundedRectangle (slot.expanded (0.5f), 3.0f, 0.8f);
        g.setColour (Colours::dim.withAlpha (0.35f * alpha));
        for (int k = 0; k <= 10; ++k)
        {
            const float ty = bottom - (bottom - top) * k / 10.0f;
            const float len = k % 5 == 0 ? 6.0f : 3.5f;
            g.drawHorizontalLine (juce::roundToInt (ty), slot.getX() - 3.0f - len, slot.getX() - 3.0f);
        }
        // level in the slot, faint
        g.setColour (Colours::brass.withAlpha (0.55f * alpha));
        g.fillRoundedRectangle (slot.reduced (1.5f).withTop (pos), 1.5f);

        auto cap = juce::Rectangle<float> (r.getCentreX() - capW * 0.5f, pos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colours::black.withAlpha (0.45f * alpha));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.5f);
        juce::ColourGradient metal (juce::Colour (0xff9a9da2).withMultipliedAlpha (alpha), cap.getX(), cap.getY(),
                                    juce::Colour (0xff3d4045).withMultipliedAlpha (alpha), cap.getX(), cap.getBottom(), false);
        metal.addColour (0.48, juce::Colour (0xff6c6f74).withMultipliedAlpha (alpha));
        metal.addColour (0.52, juce::Colour (0xff4a4d52).withMultipliedAlpha (alpha));
        g.setGradientFill (metal);
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (Colours::edge);
        g.drawRoundedRectangle (cap.reduced (0.5f), 2.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawHorizontalLine (juce::roundToInt (cap.getCentreY()), cap.getX() + 3.0f, cap.getRight() - 3.0f);
        return;
    }

    // horizontal: a slot and a small cap
    const float capW = 12.0f, capH = juce::jmin (r.getHeight() - 2.0f, 16.0f);
    auto slot = juce::Rectangle<float> (r.getX() + capW * 0.5f, r.getCentreY() - 2.5f, r.getWidth() - capW, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f * alpha));
    g.fillRoundedRectangle (slot, 2.5f);
    g.setColour (Colours::brass.withAlpha (0.6f * alpha));
    g.fillRoundedRectangle (slot.reduced (1.5f).withRight (pos), 1.5f);
    auto cap = juce::Rectangle<float> (pos - capW * 0.5f, r.getCentreY() - capH * 0.5f, capW, capH);
    juce::ColourGradient metal (juce::Colour (0xff9a9da2), cap.getX(), cap.getY(), juce::Colour (0xff3d4045), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (Colours::edge);
    g.drawRoundedRectangle (cap.reduced (0.5f), 2.5f, 1.0f);
}

void DubLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool tab = b.getProperties()["tab"];
    if (tab)
    {
        // tabs: the open one is raised and joins the page below
        if (on) drawRaised (g, r, 4.0f, true);
        else
        {
            g.setColour (over ? Colours::panel : Colours::frame.darker (0.15f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Colours::edge);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        return;
    }
    drawRaised (g, r, 4.0f, over && ! down);
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    if (on)
    {
        // lit: an amber frame like a backlit key
        g.setColour (Colours::brass.withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (1.0f), 3.5f, 1.3f);
        g.setColour (Colours::brass.withAlpha (0.08f));
        g.fillRoundedRectangle (r.reduced (1.0f), 3.5f);
    }
    if (! b.isEnabled())
    {
        g.setColour (Colours::frame.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 4.0f);
    }
}

void DubLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState(), tab = b.getProperties()["tab"];
    g.setFont (displayFont (juce::jmin (11.5f, b.getHeight() * 0.45f)));
    juce::Colour c = tab ? (on ? Colours::bright : Colours::dim) : (on ? Colours::brass : Colours::text);
    if (b.getProperties().contains ("accent") && ! on) c = juce::Colour (static_cast<juce::uint32> (static_cast<juce::int64> (b.getProperties()["accent"])));
    g.setColour (c.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void DubLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat();
    drawInset (g, r, 4.0f);
    if (box.hasKeyboardFocus (true))
    {
        g.setColour (Colours::brass.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
    juce::Path arrow;
    const float ax = static_cast<float> (w) - 13.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Colours::text.withAlpha (box.isEnabled() ? 0.8f : 0.3f));
    g.fillPath (arrow);
}

void DubLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Label* DubLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (monoFont (11.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setMinimumHorizontalScale (0.7f);
    l->setColour (juce::Label::textColourId, Colours::brass);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Slider::SliderLayout DubLookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout l;
    auto r = s.getLocalBounds();
    if (s.getTextBoxPosition() == juce::Slider::TextBoxBelow)
    {
        l.textBoxBounds = r.removeFromBottom (16);
        l.sliderBounds = r;
    }
    else
        l.sliderBounds = r;
    return l;
}

void DubLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (Colours::inset);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (Colours::brass.withAlpha (0.8f));
    g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
}

void DubLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h));
    g.setColour (Colours::frame);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (Colours::text);
    g.setFont (bodyFont (12.5f));
    g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4);
}

} // namespace dy::dub
