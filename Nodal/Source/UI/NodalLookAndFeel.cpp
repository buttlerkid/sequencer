#include "NodalLookAndFeel.h"

namespace dy::nodal {

juce::Font displayFont (float size)
{
    juce::Font f (juce::FontOptions (size, juce::Font::bold));
    f.setExtraKerningFactor (0.12f);
    return f;
}

juce::Font bodyFont (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

NodalLookAndFeel::NodalLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Colours::page);
    setColour (Label::textColourId, Colours::text);
    setColour (Slider::textBoxTextColourId, Colours::text);
    setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Colours::brass.withAlpha (0.35f));
    setColour (TextButton::buttonColourId, Colours::panel2);
    setColour (TextButton::buttonOnColourId, Colours::brass);
    setColour (TextButton::textColourOffId, Colours::text);
    setColour (TextButton::textColourOnId, Colours::ink);
    setColour (ComboBox::backgroundColourId, Colours::panel2);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::outlineColourId, Colours::line);
    setColour (ComboBox::arrowColourId, Colours::dim);
    setColour (PopupMenu::backgroundColourId, Colours::panel);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours::brass);
    setColour (PopupMenu::highlightedTextColourId, Colours::ink);
    setColour (TooltipWindow::backgroundColourId, Colours::panel2);
    setColour (TooltipWindow::textColourId, Colours::text);
    setColour (TooltipWindow::outlineColourId, Colours::line);
    setColour (TextEditor::backgroundColourId, Colours::panel2);
    setColour (TextEditor::textColourId, Colours::text);
    setColour (TextEditor::highlightColourId, Colours::brass.withAlpha (0.35f));
    setColour (TextEditor::outlineColourId, Colours::brass);
    setColour (TextEditor::focusedOutlineColourId, Colours::brass);
    setColour (CaretComponent::caretColourId, Colours::brass);
}

void NodalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.13f);
    const float arcR = radius - lineW * 0.5f;
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (Colours::line);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? a0 + static_cast<float> (s.valueToProportionOfLength (0.0)) * (a1 - a0) : a0;
    if (std::abs (ang - zero) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (zero, ang), juce::jmax (zero, ang), true);
        g.setColour (s.isEnabled() ? Colours::brass : Colours::brassDim);
        g.strokePath (v, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - lineW * 1.6f;
    g.setColour (Colours::panel2);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Colours::line);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    const juce::Point<float> p1 (c.x + std::sin (ang) * bodyR * 0.15f, c.y - std::cos (ang) * bodyR * 0.15f);
    const juce::Point<float> p2 (c.x + std::sin (ang) * bodyR * 0.82f, c.y - std::cos (ang) * bodyR * 0.82f);
    g.setColour (Colours::text);
    g.drawLine ({ p1, p2 }, juce::jmax (2.0f, lineW * 0.55f));
}

juce::Slider::SliderLayout NodalLookAndFeel::getSliderLayout (juce::Slider& s)
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

juce::Label* NodalLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (monoFont (11.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setMinimumHorizontalScale (0.7f);
    return l;
}

void NodalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (down) fill = fill.darker (0.15f);
    else if (over) fill = fill.brighter (0.08f);
    if (! b.isEnabled()) fill = fill.withAlpha (0.4f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? fill.darker (0.3f) : Colours::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void NodalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (bodyFont (juce::jmin (12.5f, b.getHeight() * 0.5f), true));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void NodalLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Colours::panel2);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Colours::brass : Colours::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = static_cast<float> (w) - 11.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 1.8f, ax + 3.5f, ay - 1.8f, ax, ay + 2.6f);
    g.setColour (Colours::dim);
    g.fillPath (arrow);
}

void NodalLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

} // namespace dy::nodal
