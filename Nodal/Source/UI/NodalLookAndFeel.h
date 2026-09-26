#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dy::nodal {

// Brass on graphite: the palette of the DY Nodal mockup.
namespace Colours
{
    inline const juce::Colour page     { 0xff0b0e13 };
    inline const juce::Colour frame    { 0xff131820 };
    inline const juce::Colour panel    { 0xff19202a };
    inline const juce::Colour panel2   { 0xff212936 };
    inline const juce::Colour line     { 0xff2b3442 };
    inline const juce::Colour text     { 0xffd9dee6 };
    inline const juce::Colour dim      { 0xff7d8797 };
    inline const juce::Colour brass    { 0xffe7b867 };
    inline const juce::Colour brassDim { 0xff7a6236 };
    inline const juce::Colour teal     { 0xff7cc3cf };
    inline const juce::Colour plate    { 0xff0e1219 };
    inline const juce::Colour ink      { 0xff1b1407 };      // text on brass
    inline const juce::Colour lilac    { 0xffb9a0e3 };      // the input follower's accent
    inline const juce::Colour scopeBg  { 0xff0e1218 };
}

juce::Font displayFont (float size);     // bold, wide tracking: captions and titles
juce::Font bodyFont (float size, bool bold = false);
juce::Font monoFont (float size);

class NodalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NodalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return bodyFont (12.5f); }
    juce::Font getPopupMenuFont() override { return bodyFont (13.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
};

} // namespace dy::nodal
