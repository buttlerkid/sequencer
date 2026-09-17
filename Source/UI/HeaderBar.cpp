#include "HeaderBar.h"
#include "Theme.h"
#include "Params/Parameters.h"

namespace dy {

// ---------------------------------------------------------------- MidiDragSource
MidiDragSource::MidiDragSource (DYSequencerProcessor& p, std::function<int()> barsProvider)
    : proc (p), bars (std::move (barsProvider))
{
    setTooltip ("Drag onto a track to drop a MIDI file of the pattern. Click to save as...");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragSource::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (dragging ? t.accent : (hover ? t.panelAlt.brighter (t.isDark ? 0.15f : 0.03f) : t.panelAlt));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (dragging ? t.accent.darker (0.2f) : t.accent);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setColour (dragging ? (t.isDark ? juce::Colour (0xff15171c) : juce::Colours::white) : t.text);
    g.setFont (uiFont (12.0f, true));
    g.drawText (juce::String::charToString (0x2913) + "  MIDI", getLocalBounds(), juce::Justification::centred);
}

void MidiDragSource::mouseDown (const juce::MouseEvent&) {}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    repaint();

    auto file = proc.exportMidiFile (bars());
    if (! file.existsAsFile())
    {
        dragging = false;
        repaint();
        return;
    }

    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [this]
    {
        dragging = false;
        repaint();
    });
}

void MidiDragSource::mouseUp (const juce::MouseEvent& e)
{
    if (dragging) return;
    if (e.mouseWasDraggedSinceMouseDown()) return;

    // A plain click saves to a file of the user's choosing.
    auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("DY Sequencer Pattern.mid");
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", suggested, "*.mid");
    const int browserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync (browserFlags, [this] (const juce::FileChooser& fc)
    {
        auto dest = fc.getResult();
        if (dest == juce::File()) return;
        if (! dest.hasFileExtension ("mid")) dest = dest.withFileExtension ("mid");
        auto src = proc.exportMidiFile (bars());
        if (src.existsAsFile())
            src.copyFileTo (dest);
    });
}

// ---------------------------------------------------------------- HeaderBar
HeaderBar::HeaderBar (DYSequencerProcessor& p)
    : proc (p),
      dragSource (p, [this] { return juce::jmax (1, exportBars.getText().getIntValue()); })
{
    setupCombo (key,          "Key",     keyNames(),            ParamIDs::key,          "Root note for all scale-mode tracks.");
    setupCombo (scale,        "Scale",   scaleNames(),          ParamIDs::scale,        "Scale for all scale-mode tracks.");
    setupCombo (swingProfile, "Shuffle", shuffleProfileNames(), ParamIDs::swingProfile, "Global shuffle profile (tracks set to Global swing).");

    swingAmount.setSliderStyle (juce::Slider::LinearHorizontal);
    swingAmount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
    swingAmount.setTooltip ("Global swing amount.");
    swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::swingAmount, swingAmount);
    addAndMakeVisible (swingAmount);
    swingLabel.setText ("Amount", juce::dontSendNotification);
    swingLabel.setFont (uiFont (11.0f));
    addAndMakeVisible (swingLabel);

    copyAll.setTooltip ("Copy all 16 tracks");
    copyAll.onClick = [this] { proc.copyPattern(); refresh(); };
    addAndMakeVisible (copyAll);

    pasteAll.setTooltip ("Paste all 16 tracks (keeps each track's channel / on / mute)");
    pasteAll.onClick = [this] { proc.pastePattern(); if (onPatternChanged) onPatternChanged(); };
    addAndMakeVisible (pasteAll);

    for (int b : { 1, 2, 4, 8, 16 })
        exportBars.addItem (juce::String (b), b);
    exportBars.setSelectedId (juce::jmax (1, proc.uiExportBars), juce::dontSendNotification);
    exportBars.setTooltip ("Bars to render when exporting MIDI.");
    exportBars.onChange = [this] { proc.uiExportBars = exportBars.getSelectedId(); };
    addAndMakeVisible (exportBars);
    barsLabel.setText ("Bars", juce::dontSendNotification);
    barsLabel.setFont (uiFont (11.0f));
    addAndMakeVisible (barsLabel);

    addAndMakeVisible (dragSource);

    themeButton.setTooltip ("Toggle light / dark theme");
    themeButton.onClick = [this] { if (onThemeToggle) onThemeToggle(); };
    addAndMakeVisible (themeButton);

    refresh();
}

void HeaderBar::setupCombo (Combo& c, const juce::String& caption, const juce::StringArray& items,
                            const juce::String& id, const juce::String& tip)
{
    c.box.addItemList (items, 1);
    c.box.setTooltip (tip);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c.box);
    addAndMakeVisible (c.box);
    c.label.setText (caption, juce::dontSendNotification);
    c.label.setFont (uiFont (11.0f));
    addAndMakeVisible (c.label);
}

void HeaderBar::refresh()
{
    themeButton.setButtonText (proc.uiTheme == 0 ? "Light" : "Dark");
    pasteAll.setEnabled (proc.hasPatternClip());
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.text);
    g.setFont (uiFont (17.0f, true));
    g.drawText ("DY", 12, 0, 40, getHeight(), juce::Justification::centredLeft);
    g.setColour (t.accent);
    g.drawText ("SEQUENCER", 42, 0, 120, getHeight(), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (0, 4);
    r.removeFromLeft (170);

    auto placeCombo = [&] (Combo& c, int w)
    {
        auto col = r.removeFromLeft (w).reduced (4, 0);
        c.label.setBounds (col.removeFromTop (14));
        c.box.setBounds (col.removeFromTop (24));
    };
    placeCombo (key, 76);
    placeCombo (scale, 140);
    placeCombo (swingProfile, 120);

    {
        auto col = r.removeFromLeft (170).reduced (4, 0);
        swingLabel.setBounds (col.removeFromTop (14));
        swingAmount.setBounds (col.removeFromTop (24));
    }

    // Right-aligned cluster
    auto right = r.removeFromRight (400);
    right.removeFromTop (14);
    right = right.removeFromTop (24);
    themeButton.setBounds (right.removeFromRight (60).reduced (3, 0));
    dragSource.setBounds (right.removeFromRight (90).reduced (3, 0));
    {
        auto barsArea = right.removeFromRight (60).reduced (3, 0);
        exportBars.setBounds (barsArea);
        barsLabel.setBounds (barsArea.withY (barsArea.getY() - 14).withHeight (14));
    }
    pasteAll.setBounds (right.removeFromRight (80).reduced (3, 0));
    copyAll.setBounds (right.removeFromRight (80).reduced (3, 0));
}

} // namespace dy
