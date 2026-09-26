#include "Theme.h"

namespace dy {

Theme Theme::dark()
{
    Theme t;
    t.isDark       = true;
    t.bg           = juce::Colour (0xff15171c);
    t.panel        = juce::Colour (0xff1e2129);
    t.panelAlt     = juce::Colour (0xff272b35);
    t.outline      = juce::Colour (0xff353a47);
    t.text         = juce::Colour (0xffe8e9ec);
    t.textDim      = juce::Colour (0xff8b909c);
    t.accent       = juce::Colour (0xfff0a33a);
    t.accentAlt    = juce::Colour (0xff4fa3ff);
    t.stepOff      = juce::Colour (0xff2a2e38);
    t.stepOn       = juce::Colour (0xfff0a33a);
    t.stepEuclid   = juce::Colour (0xff4fa3ff);
    t.stepBoth     = juce::Colour (0xff9be36b);
    t.stepDisabled = juce::Colour (0xff1b1e25);
    t.playhead     = juce::Colours::white.withAlpha (0.85f);
    t.beatShade    = juce::Colours::white.withAlpha (0.035f);
    t.bar          = juce::Colour (0xfff0a33a);
    t.barDim       = juce::Colour (0xff5a4a2c);
    t.centreLine   = juce::Colour (0xff4a5060);
    t.danger       = juce::Colour (0xffe5534b);
    t.solo         = juce::Colour (0xff4fd28a);
    t.flash        = juce::Colours::white;
    return t;
}

Theme Theme::light()
{
    Theme t;
    t.isDark       = false;
    t.bg           = juce::Colour (0xfff1f2f4);
    t.panel        = juce::Colour (0xffffffff);
    t.panelAlt     = juce::Colour (0xffe9ebef);
    t.outline      = juce::Colour (0xffd0d4db);
    t.text         = juce::Colour (0xff1e2128);
    t.textDim      = juce::Colour (0xff6b717d);
    t.accent       = juce::Colour (0xffe08a1f);
    t.accentAlt    = juce::Colour (0xff2f7fe0);
    t.stepOff      = juce::Colour (0xffdfe2e8);
    t.stepOn       = juce::Colour (0xffe08a1f);
    t.stepEuclid   = juce::Colour (0xff2f7fe0);
    t.stepBoth     = juce::Colour (0xff5cb85c);
    t.stepDisabled = juce::Colour (0xfff3f4f6);
    t.playhead     = juce::Colour (0xff1e2128).withAlpha (0.8f);
    t.beatShade    = juce::Colours::black.withAlpha (0.035f);
    t.bar          = juce::Colour (0xffe08a1f);
    t.barDim       = juce::Colour (0xffe9cfa9);
    t.centreLine   = juce::Colour (0xffb8bdc7);
    t.danger       = juce::Colour (0xffd9433b);
    t.solo         = juce::Colour (0xff2fae6a);
    t.flash        = juce::Colour (0xff1e2128);
    return t;
}

juce::Font uiFont (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

const Theme& themeOf (const juce::Component& c)
{
    if (auto* lnf = dynamic_cast<DYLookAndFeel*> (&c.getLookAndFeel()))
        return lnf->theme();
    static const Theme fallback = Theme::dark();
    return fallback;
}

ValueBox::ValueBox()
{
    // Vertical bar style = drag up/down to change, relative to the current value.
    setSliderStyle (juce::Slider::LinearBarVertical);
    setSliderSnapsToMousePosition (false);
    setMouseDragSensitivity (160);
    setTextBoxIsEditable (true);
    setScrollWheelEnabled (true);
}

void DYLookAndFeel::setTheme (const Theme& t)
{
    current = t;
    using namespace juce;

    setColourScheme ({ t.bg, t.panel, t.panel, t.outline, t.text, t.accent, t.text, t.accent, t.text });

    setColour (ResizableWindow::backgroundColourId, t.bg);
    setColour (Label::textColourId, t.text);
    setColour (Label::textWhenEditingColourId, t.text);
    setColour (Label::backgroundWhenEditingColourId, t.panelAlt);
    setColour (Label::outlineWhenEditingColourId, t.accent);

    setColour (Slider::rotarySliderFillColourId, t.accent);
    setColour (Slider::rotarySliderOutlineColourId, t.outline);
    setColour (Slider::thumbColourId, t.accent);
    setColour (Slider::trackColourId, t.accent);
    setColour (Slider::backgroundColourId, t.panelAlt);
    setColour (Slider::textBoxTextColourId, t.text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, t.accent.withAlpha (0.4f));

    setColour (TextButton::buttonColourId, t.panelAlt);
    setColour (TextButton::buttonOnColourId, t.accent);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.isDark ? Colour (0xff15171c) : Colours::white);

    setColour (ComboBox::backgroundColourId, t.panelAlt);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::outlineColourId, t.outline);
    setColour (ComboBox::arrowColourId, t.textDim);
    setColour (ComboBox::focusedOutlineColourId, t.accent);

    setColour (PopupMenu::backgroundColourId, t.panel);
    setColour (PopupMenu::textColourId, t.text);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour (PopupMenu::highlightedTextColourId, t.isDark ? Colour (0xff15171c) : Colours::white);

    setColour (TooltipWindow::backgroundColourId, t.panelAlt);
    setColour (TooltipWindow::textColourId, t.text);
    setColour (TooltipWindow::outlineColourId, t.outline);

    setColour (ScrollBar::thumbColourId, t.outline);
    setColour (ScrollBar::trackColourId, Colours::transparentBlack);
    setColour (TextEditor::textColourId, t.text);
    setColour (TextEditor::backgroundColourId, t.panelAlt);
    setColour (TextEditor::highlightColourId, t.accent.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, t.accent);
    setColour (TextEditor::focusedOutlineColourId, t.accent);
    setColour (CaretComponent::caretColourId, t.accent);
    setColour (AlertWindow::backgroundColourId, t.panel);
    setColour (AlertWindow::textColourId, t.text);
    setColour (AlertWindow::outlineColourId, t.outline);
}

void DYLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    const auto& t = current;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float lineW = juce::jmax (2.0f, radius * 0.14f);
    const float arcR  = radius - lineW / 2.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (s.isEnabled() ? t.outline : t.outline.withAlpha (0.5f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar sliders fill from the centre.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroPos = bipolar ? static_cast<float> ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
    const float fromAngle = startAngle + zeroPos * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
    g.setColour (s.isEnabled() ? t.accent : t.accent.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float dotR = lineW * 0.9f;
    const juce::Point<float> tip (centre.x + (arcR - lineW * 1.6f) * std::sin (angle),
                                  centre.y - (arcR - lineW * 1.6f) * std::cos (angle));
    g.setColour (t.text);
    g.fillEllipse (tip.x - dotR / 2, tip.y - dotR / 2, dotR, dotR);
}

void DYLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos,
                                      float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (! s.isBar())
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    // ValueBox: a flat number field with a faint fill showing where in its range it sits.
    const auto& t = current;
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (0.5f);
    g.setColour (s.isEnabled() ? t.panelAlt : t.panelAlt.withAlpha (0.4f));
    g.fillRoundedRectangle (r, 3.0f);

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const double norm = (s.getValue() - s.getMinimum()) / juce::jmax (1e-9, s.getMaximum() - s.getMinimum());
    const float zero = bipolar ? static_cast<float> ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
    const float from = r.getX() + r.getWidth() * juce::jmin (zero, static_cast<float> (norm));
    const float to   = r.getX() + r.getWidth() * juce::jmax (zero, static_cast<float> (norm));
    g.setColour (t.accent.withAlpha (s.isEnabled() ? 0.18f : 0.06f));
    g.fillRoundedRectangle (juce::Rectangle<float> (from, r.getBottom() - 3.0f, juce::jmax (1.0f, to - from), 2.5f), 1.0f);

    g.setColour (s.hasKeyboardFocus (true) ? t.accent : t.outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Label* DYLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (s.isBar() ? 11.0f : 12.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setMinimumHorizontalScale (0.7f);
    return l;
}

void DYLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                          bool isHighlighted, bool isDown)
{
    const auto& t = current;
    auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    juce::Colour fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (isDown)             fill = fill.darker (0.15f);
    else if (isHighlighted) fill = fill.brighter (t.isDark ? 0.12f : 0.04f);
    if (! b.isEnabled())    fill = fill.withAlpha (0.4f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? fill.darker (0.25f) : t.outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void DYLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto& t = current;
    auto bounds = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.isEnabled() ? t.panelAlt : t.panelAlt.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? t.accent : t.outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const float ax = static_cast<float> (w) - 11.0f, ay = h / 2.0f;
    juce::Path arrow;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (t.textDim);
    g.fillPath (arrow);
}

void DYLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void DYLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool isVertical,
                                   int thumbStart, int thumbSize, bool isMouseOver, bool)
{
    const auto& t = current;
    juce::Rectangle<int> thumb = isVertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                                            : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (isMouseOver ? t.textDim : t.outline);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

juce::Font DYLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jmin (12.5f, buttonHeight * 0.55f));
}

juce::Font DYLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont (juce::jmin (12.5f, box.getHeight() * 0.6f));
}

juce::Font DYLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

juce::Font DYLookAndFeel::getPopupMenuFont()
{
    return uiFont (13.0f);
}

} // namespace dy
