#include "PluginEditor.h"

namespace dy::mouth {

using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

// ------------------------------------------------------------------ small controls
Knob::Knob (APVTS& s, const juce::String& id, juce::String cap) : caption (std::move (cap))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
}
void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (14)); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? Colours::dim : Colours::line);
    g.setFont (displayFont (9.5f));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

Combo::Combo (APVTS& s, const juce::String& id, juce::String cap) : caption (std::move (cap))
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
}
void Combo::resized() { box.setBounds (getLocalBounds().withTrimmedTop (15)); }
void Combo::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (bodyFont (11.0f));
    g.drawText (caption, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

Toggle::Toggle (APVTS& s, const juce::String& id, juce::String text)
{
    setButtonText (text);
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

// ------------------------------------------------------------------ camera
void CameraView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::plate);
    g.fillRoundedRectangle (r, 8.0f);

    const auto v = proc.input.view();
    if (v.image.isValid())
    {
        const float iw = (float) v.image.getWidth(), ih = (float) v.image.getHeight();
        const float s = std::min ((r.getWidth() - 4.0f) / iw, (r.getHeight() - 4.0f) / ih);
        const float ox = r.getCentreX() - iw * s * 0.5f, oy = r.getCentreY() - ih * s * 0.5f;
        const bool flip = proc.mirror;
        auto t = juce::AffineTransform::scale (flip ? -s : s, s).translated (flip ? ox + iw * s : ox, oy);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<float> (ox, oy, iw * s, ih * s).toNearestInt());
            g.setOpacity (0.85f);
            g.drawImageTransformed (v.image, t);
        }
        if (v.face)
        {
            auto at = [&] (int i) { auto p = v.points[(size_t) i]; float x = p.x, y = p.y; t.transformPoint (x, y); return juce::Point<float> (x, y); };
            auto loop = [&] (const auto& idx)
            {
                juce::Path p;
                p.startNewSubPath (at (idx[0]));
                for (size_t k = 1; k < idx.size(); ++k) p.lineTo (at (idx[k]));
                p.closeSubPath();
                return p;
            };
            g.setColour (Colours::brass);
            g.strokePath (loop (Mesh::outerLips), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
            g.setColour (Colours::teal);
            g.strokePath (loop (Mesh::innerLips), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
            for (int i : { Mesh::eyeR, Mesh::eyeL })
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (at (i)));
        }
    }
    if (! v.face)
    {
        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (bodyFont (14.0f));
        const auto msg = v.status.isNotEmpty() ? v.status
                       : proc.input.deviceIndex() < 0 ? juce::String ("No camera") : juce::String ("Looking for a face");
        g.drawFittedText (msg, getLocalBounds().reduced (24).removeFromBottom (60), juce::Justification::centred, 2);
    }
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

// ------------------------------------------------------------------ meters
void ShapeMeters::paint (juce::Graphics& g)
{
    const char* names[kNumSources] = { "OPEN", "WIDTH", "SMILE", "PUCKER" };
    const bool face = proc.input.face.load();
    const float rowH = getHeight() / (float) kNumSources;
    for (int i = 0; i < kNumSources; ++i)
    {
        auto row = juce::Rectangle<float> (0, i * rowH, (float) getWidth(), rowH).reduced (0, 5);
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.0f));
        g.drawText (names[i], row.removeFromLeft (70), juce::Justification::centredLeft);
        const float v = proc.input.shape[(size_t) i].load();
        g.setFont (monoFont (11.0f));
        g.drawText (juce::String (juce::roundToInt (v * 100)) + " %", row.removeFromRight (50), juce::Justification::centredRight);
        auto bar = row.reduced (0, 4);
        g.setColour (Colours::plate);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour ((face ? Colours::brass : Colours::brassDim));
        g.fillRoundedRectangle (bar.withWidth (std::max (4.0f, bar.getWidth() * v)), 4.0f);
        if (i == Open && proc.apvts.getRawParameterValue (PID::trigger)->load() > 0.5f)
        {
            const float th = proc.apvts.getRawParameterValue (PID::threshold)->load();
            g.setColour (proc.noteOn.load() ? Colours::teal : Colours::text.withAlpha (0.6f));
            g.fillRect (bar.getX() + bar.getWidth() * th - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
        }
    }
}

// ------------------------------------------------------------------ outputs
OutputStrip::OutputStrip (MouthProcessor& p, int i)
    : proc (p), index (i),
      source (p.apvts, PID::slot (i, "Src"), "Source"), dest (p.apvts, PID::slot (i, "Dest"), "Send as"),
      cc (p.apvts, PID::slot (i, "CC"), "CC"), smooth (p.apvts, PID::slot (i, "Smooth"), "Smooth"),
      lo (p.apvts, PID::slot (i, "Lo"), "Min"), hi (p.apvts, PID::slot (i, "Hi"), "Max"),
      invert (p.apvts, PID::slot (i, "Inv"), "Invert")
{
    for (auto* c : std::initializer_list<juce::Component*> { &source, &dest, &cc, &smooth, &lo, &hi, &invert })
        addAndMakeVisible (c);
    smooth.slider.setTooltip ("Glide: higher is calmer but slower");
    invert.setTooltip ("Closed mouth sends the top of the range");
}

void OutputStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedTop (28);
    auto combos = r.removeFromTop (38);
    source.setBounds (combos.removeFromLeft (combos.getWidth() / 2 - 3));
    dest.setBounds (combos.withTrimmedLeft (6));
    r.removeFromTop (6);
    auto knobs = r.removeFromTop (80);
    const int kw = knobs.getWidth() / 4;
    for (auto* k : { &cc, &smooth, &lo, &hi }) k->setBounds (knobs.removeFromLeft (kw).reduced (2, 0));
    r.removeFromTop (8);
    invert.setBounds (r.removeFromTop (26).withSizeKeepingCentre (100, 24));
}

void OutputStrip::paint (juce::Graphics& g)
{
    auto head = getLocalBounds().reduced (10, 0).removeFromTop (22);
    g.setColour (Colours::dim);
    g.setFont (displayFont (10.5f));
    g.drawText ("OUT " + juce::String (index + 1), head.removeFromLeft (44), juce::Justification::centredLeft);
    auto bar = head.toFloat().reduced (0, 7);
    g.setColour (Colours::plate);
    g.fillRoundedRectangle (bar, 3.0f);
    const bool off = proc.apvts.getRawParameterValue (PID::slot (index, "Dest"))->load() < 0.5f;
    g.setColour (off ? Colours::line : Colours::teal);
    g.fillRoundedRectangle (bar.withWidth (std::max (3.0f, bar.getWidth() * shown)), 3.0f);
}

void OutputStrip::refresh()
{
    const int d = (int) proc.apvts.getRawParameterValue (PID::slot (index, "Dest"))->load();
    cc.setEnabled (d == DestCC);
    const float v = proc.slotOut[(size_t) index].load();
    if (std::abs (v - shown) > 0.002f) { shown = v; repaint (0, 0, getWidth(), 24); }
}

// ------------------------------------------------------------------ editor
MouthEditor::MouthEditor (MouthProcessor& p)
    : AudioProcessorEditor (p), proc (p), camera (p), meters (p),
      note (p.apvts, PID::note, "Note"), threshold (p.apvts, PID::threshold, "Threshold"),
      velocity (p.apvts, PID::velocity, "Velocity"), level (p.apvts, PID::previewLevel, "Level"),
      trigger (p.apvts, PID::trigger, "Note on open"), speed (p.apvts, PID::speed, "Velocity from speed"),
      preview (p.apvts, PID::preview, "Preview sound"), channel (p.apvts, PID::channel, "MIDI channel")
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &camera, &meters, &cameraBox, &neutral, &mirror, &note, &threshold,
                                                              &velocity, &level, &trigger, &speed, &preview, &channel })
        content.addAndMakeVisible (c);
    for (int i = 0; i < 4; ++i)
    {
        strips[(size_t) i] = std::make_unique<OutputStrip> (p, i);
        content.addAndMakeVisible (*strips[(size_t) i]);
    }

    fillCameras();
    cameraBox.onChange = [this] { if (cameraBox.getSelectedItemIndex() >= 0) proc.chooseCamera (cameraBox.getText()); };
    neutral.onClick = [this] { proc.input.captureNeutral(); };
    neutral.setTooltip ("Look at the camera with a relaxed, closed mouth and click: that face becomes zero");
    mirror.setClickingTogglesState (true);
    mirror.setToggleState (proc.mirror, juce::dontSendNotification);
    mirror.onClick = [this] { proc.mirror = mirror.getToggleState(); camera.repaint(); };
    threshold.slider.setTooltip ("How far the mouth opens before the note plays (the mark on the OPEN bar)");
    preview.setTooltip ("A built-in talk-box voice so you hear it straight away. Turn off to play another instrument from the MIDI output");

    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    proc.startCamera();
    startTimerHz (30);
}

MouthEditor::~MouthEditor() { stopTimer(); setLookAndFeel (nullptr); }

void MouthEditor::fillCameras()
{
    cameraBox.clear (juce::dontSendNotification);
    const auto devices = juce::CameraDevice::getAvailableDevices();
    cameraBox.addItemList (devices, 1);
    const int idx = proc.input.deviceIndex();
    if (idx >= 0) cameraBox.setSelectedItemIndex (idx, juce::dontSendNotification);
    cameraBox.setTextWhenNoChoicesAvailable ("No camera found");
    cameraBox.setTextWhenNothingSelected ("Camera");
}

void MouthEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::frame);
    const float s = getWidth() / (float) kW;
    g.addTransform (juce::AffineTransform::scale (s));
    g.setColour (Colours::line);
    g.fillRect (0, 46, kW, 1);
    g.setFont (displayFont (19.0f));
    g.setColour (Colours::text);
    g.drawText ("DY", 16, 0, 40, 46, juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.drawText ("MOUTH", 48, 0, 160, 46, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (monoFont (11.0f));
    g.drawText ("v" DY_VERSION_STRING "  webcam mouth to MIDI", 140, 0, 300, 46, juce::Justification::centredLeft);
    g.setColour (proc.input.face.load() ? Colours::teal : Colours::dim);
    g.drawText (headerText, kW - 420, 0, 404, 46, juce::Justification::centredRight);

    const char* titles[4] = { "CAMERA", "MOUTH", "NOTE & SOUND", "OUTPUTS" };
    for (int i = 0; i < 4; ++i)
    {
        auto r = panels[i].toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (titles[i], panels[i].getX() + 12, panels[i].getY() + 8, 200, 14, juce::Justification::centredLeft);
    }
    // dividers between the output strips
    g.setColour (Colours::line);
    for (int i = 1; i < 4; ++i)
        g.fillRect (strips[(size_t) i]->getX() - 1, panels[3].getY() + 30, 1, panels[3].getHeight() - 42);
}

void MouthEditor::resized()
{
    const float s = getWidth() / (float) kW;
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    const int pad = 16, top = 62;
    panels[0] = { pad, top, 584, 468 };
    camera.setBounds (panels[0].getX() + 12, panels[0].getY() + 28, 560, 390);
    auto row = juce::Rectangle<int> (panels[0].getX() + 12, camera.getBottom() + 8, 560, 30);
    cameraBox.setBounds (row.removeFromLeft (300));
    row.removeFromLeft (8);
    neutral.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    mirror.setBounds (row.removeFromLeft (100));

    const int x0 = panels[0].getRight() + pad, w = kW - x0 - pad;
    panels[1] = { x0, top, w, 196 };
    meters.setBounds (panels[1].reduced (14, 0).withTrimmedTop (28).withTrimmedBottom (8));

    panels[2] = { x0, panels[1].getBottom() + 12, w, panels[0].getBottom() - panels[1].getBottom() - 12 };
    auto n = panels[2].reduced (14, 0).withTrimmedTop (30);
    auto toggles = n.removeFromTop (28);
    trigger.setBounds (toggles.removeFromLeft (130));
    toggles.removeFromLeft (8);
    speed.setBounds (toggles.removeFromLeft (170));
    n.removeFromTop (8);
    auto knobs = n.removeFromTop (72);
    const int kw = knobs.getWidth() / 3;
    note.setBounds (knobs.removeFromLeft (kw));
    threshold.setBounds (knobs.removeFromLeft (kw));
    velocity.setBounds (knobs);
    n.removeFromTop (14);
    auto sound = n.removeFromTop (72);
    preview.setBounds (sound.removeFromLeft (130).withSizeKeepingCentre (130, 28));
    level.setBounds (sound.removeFromLeft (kw));
    channel.setBounds (sound.withSizeKeepingCentre (sound.getWidth() - 10, 40));

    panels[3] = { pad, panels[0].getBottom() + 12, kW - 2 * pad, kH - panels[0].getBottom() - 12 - pad };
    auto o = panels[3].withTrimmedTop (30).withTrimmedBottom (8);
    const int sw = o.getWidth() / 4;
    for (auto& st : strips) st->setBounds (o.removeFromLeft (sw));
}

void MouthEditor::timerCallback()
{
    camera.repaint();
    meters.repaint();
    for (auto& st : strips) st->refresh();
    velocity.setEnabled (proc.apvts.getRawParameterValue (PID::speed)->load() < 0.5f);

    if (++ticks % 10 == 0)
    {
        const auto idx = proc.input.deviceIndex();
        if (idx >= 0 && cameraBox.getSelectedItemIndex() != idx) cameraBox.setSelectedItemIndex (idx, juce::dontSendNotification);
        if (cameraBox.getNumItems() == 0 && ticks % 90 == 0) fillCameras();
        const auto text = idx < 0 ? juce::String ("camera off")
                        : juce::String (juce::roundToInt (proc.input.fps.load())) + " fps   " + (proc.input.face.load() ? "face found" : "no face");
        if (text != headerText)
        {
            headerText = text;
            repaint (0, 0, getWidth(), juce::roundToInt (46 * getWidth() / (float) kW));
        }
    }
}

} // namespace dy::mouth
