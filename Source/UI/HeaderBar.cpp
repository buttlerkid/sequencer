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
    g.drawText (juce::String::charToString (0x2913) + "  Export MIDI", getLocalBounds(), juce::Justification::centred);
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
    addTrack.setTooltip ("Enable the next free track (up to 16)");
    addTrack.onClick = [this] { if (onAddTrack) onAddTrack(); };
    addAndMakeVisible (addTrack);

    clearAll.setTooltip ("Clear the steps and lanes of every track (settings are kept)");
    clearAll.onClick = [this]
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Clear all tracks?")
                                          .withMessage ("This clears every step and lane on all 16 tracks. Track settings and names are kept.")
                                          .withButton ("Clear")
                                          .withButton ("Cancel")
                                          .withAssociatedComponent (this),
                                      [this] (int result)
                                      {
                                          if (result == 1)
                                          {
                                              proc.clearAll();
                                              if (onPatternChanged) onPatternChanged();
                                          }
                                      });
    };
    addAndMakeVisible (clearAll);

    copyAll.setTooltip ("Copy all 16 tracks");
    copyAll.onClick = [this] { proc.copyPattern(); refresh(); };
    addAndMakeVisible (copyAll);

    pasteAll.setTooltip ("Paste all 16 tracks (keeps each track's channel / on / mute / solo)");
    pasteAll.onClick = [this] { proc.pastePattern(); if (onPatternChanged) onPatternChanged(); };
    addAndMakeVisible (pasteAll);

    for (int b : { 1, 2, 4, 8, 16 })
        exportBars.addItem (juce::String (b), b);
    exportBars.setSelectedId (juce::jmax (1, proc.uiExportBars), juce::dontSendNotification);
    exportBars.setTooltip ("Bars to render when exporting MIDI.");
    exportBars.onChange = [this] { proc.uiExportBars = exportBars.getSelectedId(); };
    addAndMakeVisible (exportBars);
    barsLabel.setText ("bars", juce::dontSendNotification);
    barsLabel.setFont (uiFont (11.0f));
    barsLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (barsLabel);

    addAndMakeVisible (dragSource);
    refresh();
}

void HeaderBar::refresh()
{
    pasteAll.setEnabled (proc.hasPatternClip());
    addTrack.setEnabled (proc.numEnabledTracks() < kNumTracks);
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.text);
    g.setFont (uiFont (17.0f, true));
    g.drawText ("DY", 12, 0, 40, getHeight(), juce::Justification::centredLeft);
    g.setColour (t.accent);
    g.drawText ("SEQUENCER", 42, 0, 120, getHeight(), juce::Justification::centredLeft);
    g.setColour (t.textDim);
    g.setFont (uiFont (10.0f));
    g.drawText ("v" DY_VERSION_STRING, 150, 0, 60, getHeight(), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (8, 11);
    r.removeFromLeft (200);
    addTrack.setBounds (r.removeFromLeft (110));
    r.removeFromLeft (6);
    clearAll.setBounds (r.removeFromLeft (82));
    r.removeFromLeft (14);
    copyAll.setBounds (r.removeFromLeft (80));
    r.removeFromLeft (6);
    pasteAll.setBounds (r.removeFromLeft (80));

    dragSource.setBounds (r.removeFromRight (130));
    r.removeFromRight (6);
    exportBars.setBounds (r.removeFromRight (54));
    barsLabel.setBounds (r.removeFromRight (34));
}

} // namespace dy
