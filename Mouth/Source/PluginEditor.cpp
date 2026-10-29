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
        for (const auto& face : v.faces)
        {
            auto at = [&] (int i) { auto p = face.points[(size_t) i]; float x = p.x, y = p.y; t.transformPoint (x, y); return juce::Point<float> (x, y); };
            auto loop = [&] (const auto& idx)
            {
                juce::Path p;
                p.startNewSubPath (at (idx[0]));
                for (size_t k = 1; k < idx.size(); ++k) p.lineTo (at (idx[k]));
                p.closeSubPath();
                return p;
            };
            const auto col = faceColour (face.id);
            const bool singing = proc.noteOf[(size_t) face.id].load() >= 0;
            const auto inner = loop (Mesh::innerLips);
            if (singing) { g.setColour (col.withAlpha (0.45f)); g.fillPath (inner); }
            g.setColour (col);
            g.strokePath (loop (Mesh::outerLips), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
            g.setColour (col.brighter (0.4f));
            g.strokePath (inner, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
            for (int i : { Mesh::eyeR, Mesh::eyeL })
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (at (i)));
            // the face's number, above its brows
            const auto top = (at (Mesh::browR) + at (Mesh::browL)) * 0.5f;
            const float eyes = at (Mesh::eyeR).getDistanceFrom (at (Mesh::eyeL));
            auto badge = juce::Rectangle<float> (22.0f, 22.0f).withCentre (top.translated (0.0f, -std::max (14.0f, eyes * 0.45f)));
            g.setColour (col);
            g.fillEllipse (badge);
            g.setColour (Colours::ink);
            g.setFont (displayFont (12.0f));
            g.drawText (juce::String (face.id + 1), badge, juce::Justification::centred);
        }
    }
    if (v.faces.empty())
    {
        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (bodyFont (14.0f));
        const auto msg = v.status.isNotEmpty() ? v.status
                       : proc.input.deviceIndex() < 0 ? juce::String ("No camera") : juce::String ("Looking for faces");
        g.drawFittedText (msg, getLocalBounds().reduced (24).removeFromBottom (60), juce::Justification::centred, 2);
    }
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

// ------------------------------------------------------------------ meters
void ShapeMeters::paint (juce::Graphics& g)
{
    const auto names = sourceNames();
    const auto faces = proc.input.faces();
    const bool trig = proc.apvts.getRawParameterValue (PID::trigger)->load() > 0.5f;
    const float th = proc.apvts.getRawParameterValue (PID::threshold)->load();
    const float labelW = 64.0f, colW = (getWidth() - labelW) / (float) kMaxFaces;
    const float headH = 16.0f, rowH = (getHeight() - headH) / (float) kNumSources;

    g.setFont (displayFont (9.5f));
    for (int f = 0; f < kMaxFaces; ++f)
    {
        g.setColour (faces.present[(size_t) f] ? faceColour (f) : Colours::line);
        g.drawText ("FACE " + juce::String (f + 1), juce::Rectangle<float> (labelW + f * colW, 0, colW, headH), juce::Justification::centred);
    }
    for (int i = 0; i < kNumSources; ++i)
    {
        const float y = headH + i * rowH;
        g.setColour (Colours::dim);
        g.setFont (displayFont (9.5f));
        g.drawText (names[i].toUpperCase(), juce::Rectangle<float> (0, y, labelW, rowH), juce::Justification::centredLeft);
        for (int f = 0; f < kMaxFaces; ++f)
        {
            auto bar = juce::Rectangle<float> (labelW + f * colW, y, colW, rowH).reduced (4.0f, rowH * 0.22f);
            g.setColour (Colours::plate);
            g.fillRoundedRectangle (bar, 3.0f);
            if (! faces.present[(size_t) f]) continue;
            const float v = faces.shapes[(size_t) f][(size_t) i];
            g.setColour (faceColour (f));
            if (i == Tilt || i == HeadX)     // centred: a line from the middle
                g.fillRect (juce::Rectangle<float> (bar.getX() + bar.getWidth() * std::min (v, 0.5f), bar.getY(),
                                                    std::max (2.0f, bar.getWidth() * std::abs (v - 0.5f)), bar.getHeight()));
            else
                g.fillRoundedRectangle (bar.withWidth (std::max (3.0f, bar.getWidth() * v)), 3.0f);
            if (i == Open && trig)
            {
                g.setColour (proc.noteOf[(size_t) f].load() >= 0 ? Colours::text : Colours::text.withAlpha (0.5f));
                g.fillRect (bar.getX() + bar.getWidth() * th - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
            }
        }
    }
}

// ------------------------------------------------------------------ outputs
OutputStrip::OutputStrip (MouthProcessor& p, int i)
    : proc (p), index (i),
      source (p.apvts, PID::slot (i, "Src"), "Source"), dest (p.apvts, PID::slot (i, "Dest"), "Send as"),
      cc (p.apvts, PID::slot (i, "CC"), "CC"), smooth (p.apvts, PID::slot (i, "Smooth"), "Smooth"),
      lo (p.apvts, PID::slot (i, "Lo"), "Min"), hi (p.apvts, PID::slot (i, "Hi"), "Max"),
      invert (p.apvts, PID::slot (i, "Inv"), "Invert"), face (p.apvts, PID::slot (i, "Face"), "Face")
{
    for (auto* c : std::initializer_list<juce::Component*> { &source, &dest, &cc, &smooth, &lo, &hi, &invert, &face })
        addAndMakeVisible (c);
    smooth.slider.setTooltip ("Glide: higher is calmer but slower");
    invert.setTooltip ("Closed mouth sends the top of the range");
}

void OutputStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedTop (28);
    auto combos = r.removeFromTop (38);
    face.setBounds (combos.removeFromLeft (70));
    combos.removeFromLeft (5);
    source.setBounds (combos.removeFromLeft (combos.getWidth() / 2 - 3));
    dest.setBounds (combos.withTrimmedLeft (5));
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
    const int f = (int) proc.apvts.getRawParameterValue (PID::slot (index, "Face"))->load();
    g.setColour (off ? Colours::line : faceColour (f));
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
      preview (p.apvts, PID::preview, "Preview sound"), faceChannels (p.apvts, PID::faceChannels, "Face = channel"),
      channel (p.apvts, PID::channel, "MIDI channel"), pitchMode (p.apvts, PID::pitchMode, "Pitch"),
      chord (p.apvts, PID::chord, "Chord"), scale (p.apvts, PID::scale, "Scale"), faces (p.apvts, PID::faces, "Faces")
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &camera, &meters, &cameraBox, &neutral, &mirror, &note, &threshold,
                                                              &velocity, &level, &trigger, &speed, &preview, &channel,
                                                              &faceChannels, &pitchMode, &chord, &scale, &faces })
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
    pitchMode.box.setTooltip ("Chord: the faces share a chord out, face 1 the root. Head height: each face's note goes up "
                              "the scale as the head rises, two octaves from the note");
    faces.box.setTooltip ("How many faces to follow at once (fewer is lighter on the CPU)");
    faceChannels.setTooltip ("Face 1 plays on the MIDI channel, face 2 on the next one and so on: one instrument per face");
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
    g.setColour (proc.input.faceCount() > 0 ? Colours::teal : Colours::dim);
    g.drawText (headerText, kW - 420, 0, 404, 46, juce::Justification::centredRight);

    const char* titles[4] = { "CAMERA", "FACES", "NOTES & SOUND", "OUTPUTS" };
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
    panels[0] = { pad, top, 584, 498 };
    camera.setBounds (panels[0].getX() + 12, panels[0].getY() + 28, 560, 420);
    auto row = juce::Rectangle<int> (panels[0].getX() + 12, camera.getBottom() + 8, 560, 30);
    cameraBox.setBounds (row.removeFromLeft (300));
    row.removeFromLeft (8);
    neutral.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    mirror.setBounds (row.removeFromLeft (100));

    const int x0 = panels[0].getRight() + pad, w = kW - x0 - pad;
    panels[1] = { x0, top, w, 236 };
    meters.setBounds (panels[1].reduced (14, 0).withTrimmedTop (28).withTrimmedBottom (8));

    panels[2] = { x0, panels[1].getBottom() + 12, w, panels[0].getBottom() - panels[1].getBottom() - 12 };
    auto n = panels[2].reduced (14, 0).withTrimmedTop (30);
    auto toggles = n.removeFromTop (28);
    trigger.setBounds (toggles.removeFromLeft (108));
    toggles.removeFromLeft (8);
    speed.setBounds (toggles.removeFromLeft (146));
    toggles.removeFromLeft (8);
    preview.setBounds (toggles);
    n.removeFromTop (6);
    auto combos = n.removeFromTop (40);
    const int cw = (combos.getWidth() - 16) / 3;
    pitchMode.setBounds (combos.removeFromLeft (cw));
    combos.removeFromLeft (8);
    chord.setBounds (combos.removeFromLeft (cw));
    scale.setBounds (chord.getBounds());
    combos.removeFromLeft (8);
    faces.setBounds (combos);
    n.removeFromTop (8);
    auto knobs = n.removeFromTop (72);
    const int kw = knobs.getWidth() / 4;
    note.setBounds (knobs.removeFromLeft (kw));
    threshold.setBounds (knobs.removeFromLeft (kw));
    velocity.setBounds (knobs.removeFromLeft (kw));
    level.setBounds (knobs);
    n.removeFromTop (4);
    auto bottom = n.removeFromTop (40);
    channel.setBounds (bottom.removeFromLeft (cw));
    bottom.removeFromLeft (8);
    faceChannels.setBounds (bottom.removeFromBottom (25).removeFromLeft (130));

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
    const bool height = proc.apvts.getRawParameterValue (PID::pitchMode)->load() > 0.5f;
    chord.setVisible (! height);
    scale.setVisible (height);

    if (++ticks % 10 == 0)
    {
        const auto idx = proc.input.deviceIndex();
        if (idx >= 0 && cameraBox.getSelectedItemIndex() != idx) cameraBox.setSelectedItemIndex (idx, juce::dontSendNotification);
        if (cameraBox.getNumItems() == 0 && ticks % 90 == 0) fillCameras();
        const auto text = idx < 0 ? juce::String ("camera off")
                        : juce::String (juce::roundToInt (proc.input.fps.load())) + " fps   " + [&] { const int n = proc.input.faceCount(); return n == 0 ? juce::String ("no face") : n == 1 ? juce::String ("1 face") : juce::String (n) + " faces"; }();
        if (text != headerText)
        {
            headerText = text;
            repaint (0, 0, getWidth(), juce::roundToInt (46 * getWidth() / (float) kW));
        }
    }
}

} // namespace dy::mouth
