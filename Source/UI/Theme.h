#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dy {

struct Theme
{
    juce::Colour bg, panel, panelAlt, outline;
    juce::Colour text, textDim;
    juce::Colour accent, accentAlt;
    juce::Colour stepOff, stepOn, stepEuclid, stepBoth, stepDisabled;
    juce::Colour playhead, beatShade;
    juce::Colour bar, barDim, centreLine;
    juce::Colour danger, solo, flash;
    bool isDark = true;

    static Theme dark();
    static Theme light();
};

class DYLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit DYLookAndFeel (const Theme& t) { setTheme (t); }

    void setTheme (const Theme& t);
    const Theme& theme() const { return current; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool isVertical,
                        int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;

private:
    Theme current;
};

// Theme of the look-and-feel currently applied to a component (falls back to dark).
const Theme& themeOf (const juce::Component& c);

juce::Font uiFont (float size, bool bold = false);

// A compact draggable number box (horizontal bar slider, relative drag, double-click to type).
class ValueBox : public juce::Slider
{
public:
    ValueBox();
};

} // namespace dy
