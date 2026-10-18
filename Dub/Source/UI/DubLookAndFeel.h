#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "NodalLookAndFeel.h"          // the suite's fonts

namespace dy::dub {

using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

// Charcoal hardware with amber: the DY Dub console.
namespace Colours
{
    inline const juce::Colour page     { 0xff121315 };
    inline const juce::Colour frame    { 0xff232528 };      // the body
    inline const juce::Colour frameTop { 0xff2d2f33 };
    inline const juce::Colour panel    { 0xff292b2f };      // raised strips / sections
    inline const juce::Colour panel2   { 0xff33363b };
    inline const juce::Colour inset    { 0xff1b1c1f };      // recessed fields and slots
    inline const juce::Colour line     { 0xff3c3f45 };
    inline const juce::Colour edge     { 0xff111214 };      // dark bevel edge
    inline const juce::Colour text     { 0xffd6d8dc };
    inline const juce::Colour bright   { 0xfff2f3f5 };
    inline const juce::Colour dim      { 0xff80848c };
    inline const juce::Colour brass    { 0xffe8b923 };      // amber: values, arcs, lit buttons
    inline const juce::Colour brassDim { 0xff6b5618 };
    inline const juce::Colour teal     { 0xff6fc2d0 };
    inline const juce::Colour lilac    { 0xffb9a0e3 };
    inline const juce::Colour ink      { 0xff1a1405 };
    inline const juce::Colour scopeBg  { 0xff17181b };
}

// Draw helpers shared by the editor's own components.
void drawInset (juce::Graphics&, juce::Rectangle<float>, float corner = 6.0f);       // recessed area
void drawRaised (juce::Graphics&, juce::Rectangle<float>, float corner = 6.0f, bool lit = false);
void drawKnob (juce::Graphics&, juce::Rectangle<float>, float pos, float a0, float a1, bool bipolarAtZero, float zeroPos,
               bool enabled, const juce::NamedValueSet* props = nullptr);
void drawLogo (juce::Graphics&, juce::Rectangle<float>);

class DubLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DubLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return bodyFont (13.0f); }
    juce::Font getPopupMenuFont() override { return bodyFont (13.5f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return monoFont (12.0f); }
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
};

} // namespace dy::dub
