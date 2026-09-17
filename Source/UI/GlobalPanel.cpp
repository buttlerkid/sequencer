#include "GlobalPanel.h"
#include "Theme.h"
#include "Params/Parameters.h"

namespace dy {

GlobalPanel::GlobalPanel (DYSequencerProcessor& p) : proc (p)
{
    setupCombo (key,     "Key",     keyNames(),            ParamIDs::key,          "Root note for all Scale-mode tracks.");
    setupCombo (scale,   "Scale",   scaleNames(),          ParamIDs::scale,        "Scale for all Scale-mode tracks. Chromatic = no quantisation.");
    setupCombo (shuffle, "Shuffle", shuffleProfileNames(), ParamIDs::swingProfile, "Global shuffle profile (tracks set to Global).");
    setupSlider (swingAmount, "Amount",       ParamIDs::swingAmount, "Global shuffle amount.");
    setupSlider (masterShift, "Master shift", ParamIDs::masterShift, "Moves every track in time (ms). Negative = earlier.");
    masterShift.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1); };
    masterShift.slider.updateText();

    themeButton.setTooltip ("Toggle light / dark theme");
    themeButton.onClick = [this] { if (onThemeToggle) onThemeToggle(); };
    addAndMakeVisible (themeButton);
    refresh();
}

void GlobalPanel::setupCombo (Combo& c, const juce::String& caption, const juce::StringArray& items,
                              const juce::String& id, const juce::String& tip)
{
    c.box.addItemList (items, 1);
    c.box.setTooltip (tip);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c.box);
    addAndMakeVisible (c.box);
    c.label.setText (caption, juce::dontSendNotification);
    c.label.setFont (uiFont (10.5f));
    addAndMakeVisible (c.label);
}

void GlobalPanel::setupSlider (HSlider& s, const juce::String& caption, const juce::String& id, const juce::String& tip)
{
    s.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    s.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    s.slider.setTooltip (tip);
    s.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s.slider);
    addAndMakeVisible (s.slider);
    s.label.setText (caption, juce::dontSendNotification);
    s.label.setFont (uiFont (10.5f));
    addAndMakeVisible (s.label);
}

void GlobalPanel::refresh()
{
    themeButton.setButtonText (proc.uiTheme == 0 ? "Light theme" : "Dark theme");
}

void GlobalPanel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (t.textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("GLOBAL", 10, 4, 100, 20, juce::Justification::centredLeft);
}

void GlobalPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromTop (20);

    auto placeCombo = [&] (Combo& c, juce::Rectangle<int> area)
    {
        c.label.setBounds (area.removeFromTop (13));
        c.box.setBounds (area.removeFromTop (22));
    };
    {
        auto row = r.removeFromTop (36);
        auto keyArea = row.removeFromLeft (70);
        row.removeFromLeft (6);
        placeCombo (key, keyArea);
        placeCombo (scale, row);
    }
    r.removeFromTop (4);
    placeCombo (shuffle, r.removeFromTop (36));
    r.removeFromTop (4);

    auto placeSlider = [&] (HSlider& s)
    {
        auto row = r.removeFromTop (34);
        s.label.setBounds (row.removeFromTop (13));
        s.slider.setBounds (row.removeFromTop (20));
        r.removeFromTop (2);
    };
    placeSlider (swingAmount);
    placeSlider (masterShift);

    themeButton.setBounds (r.removeFromBottom (24));
}

} // namespace dy
