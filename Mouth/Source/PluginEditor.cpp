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
        auto map = [&] (Point p) { float x = p.x, y = p.y; t.transformPoint (x, y); return juce::Point<float> (x, y); };
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<float> (ox, oy, iw * s, ih * s).toNearestInt());
            g.setOpacity (0.85f);
            g.drawImageTransformed (v.image, t);
        }
        auto badge = [&] (juce::Point<float> c, juce::Colour col, const juce::String& text)
        {
            auto b = juce::Rectangle<float> (24.0f, 22.0f).withCentre (c);
            g.setColour (col);
            g.fillRoundedRectangle (b, 11.0f);
            g.setColour (Colours::ink);
            g.setFont (displayFont (11.0f));
            g.drawText (text, b, juce::Justification::centred);
        };

        for (const auto& face : v.faces)
        {
            auto at = [&] (int i) { return map (face.points[(size_t) i]); };
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
            // its number, above the brows
            const auto top = (at (Mesh::browR) + at (Mesh::browL)) * 0.5f;
            const float eyes = at (Mesh::eyeR).getDistanceFrom (at (Mesh::eyeL));
            badge (top.translated (0.0f, -std::max (14.0f, eyes * 0.45f)), col, juce::String (face.id + 1));
        }

        for (const auto& hand : v.hands)
        {
            const auto col = faceColour (handWho (hand.id));
            // the palm's path: a fading tail, brighter while it circles
            if (hand.trail.size() > 1)
                for (size_t k = 1; k < hand.trail.size(); ++k)
                {
                    const float a = (float) k / (float) hand.trail.size();
                    g.setColour (col.withAlpha (a * (0.25f + 0.75f * hand.circle)));
                    g.drawLine ({ map (hand.trail[k - 1]), map (hand.trail[k]) }, 1.5f + 4.0f * a * hand.circle);
                }
            g.setColour (col);
            for (auto& b : HandMesh::bones)
                g.drawLine ({ map (hand.points[(size_t) b[0]]), map (hand.points[(size_t) b[1]]) }, 2.0f);
            for (auto& p : hand.points)
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (map (p)));
            juce::Point<float> palm;
            for (int i : { HandMesh::wrist, HandMesh::indexKnuckle, HandMesh::middleKnuckle, HandMesh::ringKnuckle, HandMesh::pinkyKnuckle })
                palm += map (hand.points[(size_t) i]) * 0.2f;
            // a circle just finished: a ring flies out from the palm
            if (hand.lastCircle < 0.4)
            {
                const float k = (float) (hand.lastCircle / 0.4);
                g.setColour (col.withAlpha (1.0f - k));
                g.drawEllipse (juce::Rectangle<float> (30.0f + 160.0f * k, 30.0f + 160.0f * k).withCentre (palm), 4.0f * (1.0f - k) + 1.0f);
            }
            badge (map (hand.points[HandMesh::wrist]).translated (0.0f, 18.0f), col, "H" + juce::String (hand.id + 1));
        }
    }
    if (v.faces.empty() && v.hands.empty())
    {
        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (bodyFont (14.0f));
        const auto msg = v.status.isNotEmpty() ? v.status
                       : proc.input.deviceIndex() < 0 ? juce::String ("No camera") : juce::String ("Looking for faces and hands");
        g.drawFittedText (msg, getLocalBounds().reduced (24).removeFromBottom (60), juce::Justification::centred, 2);
    }
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

// ------------------------------------------------------------------ meters
void ShapeMeters::paint (juce::Graphics& g)
{
    const auto names = sourceNames();
    const auto scene = proc.input.scene();
    const bool trig = proc.apvts.getRawParameterValue (PID::trigger)->load() > 0.5f;
    const float th = proc.apvts.getRawParameterValue (PID::threshold)->load();
    const float labelW = 64.0f, colW = (getWidth() - labelW) / (float) nWho;
    const float headH = 16.0f, rowH = (getHeight() - headH) / (float) nSources;

    g.setFont (displayFont (9.5f));
    for (int c = 0; c < nWho; ++c)
    {
        const int w = who0 + c;
        g.setColour (scene.present[(size_t) w] ? faceColour (w) : Colours::line);
        g.drawText (w < kMaxFaces ? "FACE " + juce::String (w + 1) : "HAND " + juce::String (w - kMaxFaces + 1),
                    juce::Rectangle<float> (labelW + c * colW, 0, colW, headH), juce::Justification::centred);
    }
    for (int i = 0; i < nSources; ++i)
    {
        const int src = source0 + i;
        const float y = headH + i * rowH;
        g.setColour (Colours::dim);
        g.setFont (displayFont (9.5f));
        g.drawText (names[src].toUpperCase(), juce::Rectangle<float> (0, y, labelW, rowH), juce::Justification::centredLeft);
        for (int c = 0; c < nWho; ++c)
        {
            const int w = who0 + c;
            auto bar = juce::Rectangle<float> (labelW + c * colW, y, colW, rowH).reduced (4.0f, rowH * 0.22f);
            g.setColour (Colours::plate);
            g.fillRoundedRectangle (bar, 3.0f);
            if (! scene.present[(size_t) w]) continue;
            const float v = scene.shapes[(size_t) w][(size_t) src];
            g.setColour (faceColour (w));
            if (src == Tilt || src == HeadX || src == HandX || src == Spin)     // centred: a bar from the middle
                g.fillRect (juce::Rectangle<float> (bar.getX() + bar.getWidth() * std::min (v, 0.5f), bar.getY(),
                                                    std::max (2.0f, bar.getWidth() * std::abs (v - 0.5f)), bar.getHeight()));
            else
                g.fillRoundedRectangle (bar.withWidth (std::max (3.0f, bar.getWidth() * v)), 3.0f);
            if (src == Open && trig)
            {
                g.setColour (proc.noteOf[(size_t) w].load() >= 0 ? Colours::text : Colours::text.withAlpha (0.5f));
                g.fillRect (bar.getX() + bar.getWidth() * th - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
            }
        }
    }
}

// ------------------------------------------------------------------ the take
void TakeDrag::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto& rec = proc.recorder;
    const bool live = rec.isRecording(), have = rec.notes() > 0 || rec.length() > 0.0;
    g.setColour (live ? juce::Colour (0xffc8483c).withAlpha (0.25f) : have ? Colours::brass.withAlpha (0.18f) : Colours::plate);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (live ? juce::Colour (0xffe0574a) : have ? Colours::brass : Colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.5f);
    g.setFont (bodyFont (12.0f));
    g.setColour (Colours::text);
    const auto len = juce::String (rec.length(), 1) + " s, " + juce::String (rec.notes()) + " notes";
    g.drawFittedText (live ? "Recording  " + len
                           : have ? "Drag the take into Live  (" + len + ")"
                                  : "Record, play, stop: then drag the take into Live as a clip",
                      getLocalBounds().reduced (8, 2), juce::Justification::centred, 2);
}

void TakeDrag::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4 || proc.recorder.isRecording()) return;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("DY Mouth take " + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".mid");
    if (! proc.recorder.writeTo (file)) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ------------------------------------------------------------------ outputs
OutputStrip::OutputStrip (MouthProcessor& p, int i)
    : proc (p), index (i),
      who (p.apvts, PID::slot (i, "Face"), "Who"), source (p.apvts, PID::slot (i, "Src"), "Source"), dest (p.apvts, PID::slot (i, "Dest"), "Send as"),
      cc (p.apvts, PID::slot (i, "CC"), "CC"), smooth (p.apvts, PID::slot (i, "Smooth"), "Smooth"),
      lo (p.apvts, PID::slot (i, "Lo"), "Min"), hi (p.apvts, PID::slot (i, "Hi"), "Max"),
      invert (p.apvts, PID::slot (i, "Inv"), "Invert")
{
    for (auto* c : std::initializer_list<juce::Component*> { &who, &source, &dest, &cc, &smooth, &lo, &hi, &invert })
        addAndMakeVisible (c);
    smooth.slider.setTooltip ("Glide: higher is calmer but slower");
    invert.setTooltip ("Closed mouth (or a still hand) sends the top of the range");
}

void OutputStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedTop (28);
    auto combos = r.removeFromTop (38);
    who.setBounds (combos.removeFromLeft (76));
    combos.removeFromLeft (5);
    source.setBounds (combos.removeFromLeft ((combos.getWidth() - 5) / 2));
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
    g.setColour (off ? Colours::line : faceColour (std::max (0, shownWho)));
    g.fillRoundedRectangle (bar.withWidth (std::max (3.0f, bar.getWidth() * shown)), 3.0f);
}

void OutputStrip::refresh()
{
    const int d = (int) proc.apvts.getRawParameterValue (PID::slot (index, "Dest"))->load();
    cc.setEnabled (d == DestCC);
    // a face has face shapes, a hand hand shapes
    const int w = (int) proc.apvts.getRawParameterValue (PID::slot (index, "Face"))->load();
    if (w != shownWho)
    {
        shownWho = w;
        for (int s = 0; s < kNumSources; ++s)
            source.box.setItemEnabled (s + 1, isHandSource (s) == (w >= kMaxFaces));
        repaint();
    }
    const float v = proc.slotOut[(size_t) index].load();
    if (std::abs (v - shown) > 0.002f) { shown = v; repaint (0, 0, getWidth(), 24); }
}

// ------------------------------------------------------------------ editor
MouthEditor::MouthEditor (MouthProcessor& p)
    : AudioProcessorEditor (p), proc (p), camera (p),
      faceMeters (p, Open, HandX - Open, 0, kMaxFaces), handMeters (p, HandX, kNumSources - HandX, kMaxFaces, kMaxHands),
      note (p.apvts, PID::note, "Note"), threshold (p.apvts, PID::threshold, "Threshold"),
      velocity (p.apvts, PID::velocity, "Velocity"), level (p.apvts, PID::previewLevel, "Level"),
      circleNote (p.apvts, PID::circleNote, "Circle note"), circleLength (p.apvts, PID::circleLength, "Length"),
      trigger (p.apvts, PID::trigger, "Note on open"), speed (p.apvts, PID::speed, "Velocity from speed"),
      preview (p.apvts, PID::preview, "Preview sound"), faceChannels (p.apvts, PID::faceChannels, "Face = channel"),
      circleNotes (p.apvts, PID::circleNotes, "Circles play"),
      channel (p.apvts, PID::channel, "MIDI channel"), pitchMode (p.apvts, PID::pitchMode, "Pitch"),
      chord (p.apvts, PID::chord, "Chord"), scale (p.apvts, PID::scale, "Scale"), faces (p.apvts, PID::faces, "Faces"),
      hands (p.apvts, PID::hands, "Hands"), take (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &camera, &faceMeters, &handMeters, &cameraBox, &neutral, &mirror, &record,
                                                              &note, &threshold, &velocity, &level, &circleNote, &circleLength,
                                                              &trigger, &speed, &preview, &faceChannels, &circleNotes,
                                                              &channel, &pitchMode, &chord, &scale, &faces, &hands, &take })
        content.addAndMakeVisible (c);
    for (int i = 0; i < 4; ++i)
    {
        strips[(size_t) i] = std::make_unique<OutputStrip> (p, i);
        content.addAndMakeVisible (*strips[(size_t) i]);
    }

    fillCameras();
    cameraBox.onChange = [this] { if (cameraBox.getSelectedItemIndex() >= 0) proc.chooseCamera (cameraBox.getText()); };
    neutral.onClick = [this] { proc.input.captureNeutral(); };
    neutral.setTooltip ("Everyone looks at the camera with a relaxed, closed mouth: that face becomes zero");
    mirror.setClickingTogglesState (true);
    mirror.setToggleState (proc.mirror, juce::dontSendNotification);
    mirror.onClick = [this] { proc.mirror = mirror.getToggleState(); camera.repaint(); };
    record.setClickingTogglesState (true);
    record.onClick = [this] { proc.setRecording (record.getToggleState()); take.repaint(); };
    record.setTooltip ("Record everything DY Mouth plays; then drag the take into Live as a MIDI clip");
    threshold.slider.setTooltip ("How far the mouth opens before the note plays (the mark on the OPEN bars)");
    pitchMode.box.setTooltip ("Chord: the faces share a chord out, face 1 the root. Head height: each face's note goes up "
                              "the scale as the head rises, two octaves from the note");
    faces.box.setTooltip ("How many faces to follow at once (fewer is lighter on the CPU)");
    hands.box.setTooltip ("How many hands to follow (they run on their own core; Off saves it)");
    circleNotes.setTooltip ("Every circle a hand draws plays a note: big circles low, small ones high, fast ones loud");
    circleNote.slider.setTooltip ("The note of the biggest circle; smaller ones climb two octaves of the scale");
    faceChannels.setTooltip ("Face 1 plays on the MIDI channel, face 2 on the next and so on (hands after the faces)");
    preview.setTooltip ("Built-in talk-box voices and a bell for the circles, so you hear it straight away. "
                        "Turn off to play another instrument from the MIDI output");

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
    g.drawText ("v" DY_VERSION_STRING "  faces & hands to MIDI", 140, 0, 320, 46, juce::Justification::centredLeft);
    g.setColour (proc.input.faceCount() + proc.input.handCount() > 0 ? Colours::teal : Colours::dim);
    g.drawText (headerText, kW - 520, 0, 504, 46, juce::Justification::centredRight);

    const char* titles[6] = { "CAMERA", "FACES", "HANDS", "NOTES & SOUND", "HAND NOTES & TAKE", "OUTPUTS" };
    for (int i = 0; i < 6; ++i)
    {
        auto r = panels[i].toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (titles[i], panels[i].getX() + 12, panels[i].getY() + 8, 260, 14, juce::Justification::centredLeft);
    }
    // dividers between the output strips
    g.setColour (Colours::line);
    for (int i = 1; i < 4; ++i)
        g.fillRect (strips[(size_t) i]->getX() - 1, panels[5].getY() + 30, 1, panels[5].getHeight() - 42);
}

void MouthEditor::resized()
{
    const float s = getWidth() / (float) kW;
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    const int pad = 16, top = 62, gap = 12;
    // camera
    panels[0] = { pad, top, 584, 498 };
    camera.setBounds (panels[0].getX() + 12, panels[0].getY() + 28, 560, 420);
    auto row = juce::Rectangle<int> (panels[0].getX() + 12, camera.getBottom() + 8, 560, 30);
    cameraBox.setBounds (row.removeFromLeft (300));
    row.removeFromLeft (8);
    neutral.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    mirror.setBounds (row.removeFromLeft (100));

    // meters
    const int x1 = panels[0].getRight() + pad, w1 = 300;
    panels[1] = { x1, top, w1, 290 };
    faceMeters.setBounds (panels[1].reduced (14, 0).withTrimmedTop (28).withTrimmedBottom (8));
    panels[2] = { x1, panels[1].getBottom() + gap, w1, panels[0].getBottom() - panels[1].getBottom() - gap };
    handMeters.setBounds (panels[2].reduced (14, 0).withTrimmedTop (28).withTrimmedBottom (8));

    // notes & sound
    const int x2 = panels[1].getRight() + pad, w2 = kW - x2 - pad;
    panels[3] = { x2, top, w2, 266 };
    auto n = panels[3].reduced (14, 0).withTrimmedTop (30);
    auto toggles = n.removeFromTop (28);
    trigger.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2 - 4));
    preview.setBounds (toggles.withTrimmedLeft (8));
    n.removeFromTop (6);
    auto combos = n.removeFromTop (40);
    const int cw = (combos.getWidth() - 16) / 3;
    pitchMode.setBounds (combos.removeFromLeft (cw));
    combos.removeFromLeft (8);
    chord.setBounds (combos.removeFromLeft (cw));
    scale.setBounds (chord.getBounds());
    combos.removeFromLeft (8);
    faces.setBounds (combos);
    n.removeFromTop (6);
    auto knobs = n.removeFromTop (72);
    const int kw = knobs.getWidth() / 4;
    note.setBounds (knobs.removeFromLeft (kw));
    threshold.setBounds (knobs.removeFromLeft (kw));
    velocity.setBounds (knobs.removeFromLeft (kw));
    level.setBounds (knobs);
    n.removeFromTop (6);
    auto t2 = n.removeFromTop (26);
    speed.setBounds (t2.removeFromLeft (t2.getWidth() / 2 + 20));
    faceChannels.setBounds (t2.withTrimmedLeft (8));
    n.removeFromTop (2);
    channel.setBounds (n.removeFromTop (40).removeFromLeft (cw));

    // hand notes & the take
    panels[4] = { x2, panels[3].getBottom() + gap, w2, panels[0].getBottom() - panels[3].getBottom() - gap };
    auto h = panels[4].reduced (14, 0).withTrimmedTop (30);
    auto hrow = h.removeFromTop (72);
    auto hleft = hrow.removeFromLeft (cw + 10);
    hands.setBounds (hleft.removeFromTop (40));
    hleft.removeFromTop (4);
    circleNotes.setBounds (hleft.removeFromTop (26));
    hrow.removeFromLeft (8);
    circleNote.setBounds (hrow.removeFromLeft (hrow.getWidth() / 2));
    circleLength.setBounds (hrow);
    h.removeFromTop (12);
    auto trow = h.removeFromTop (50);
    record.setBounds (trow.removeFromLeft (80).withSizeKeepingCentre (80, 30));
    trow.removeFromLeft (8);
    take.setBounds (trow);

    // outputs
    panels[5] = { pad, panels[0].getBottom() + gap, kW - 2 * pad, kH - panels[0].getBottom() - gap - pad };
    auto o = panels[5].withTrimmedTop (30).withTrimmedBottom (8);
    const int sw = o.getWidth() / 4;
    for (auto& st : strips) st->setBounds (o.removeFromLeft (sw));
}

void MouthEditor::timerCallback()
{
    camera.repaint();
    faceMeters.repaint();
    handMeters.repaint();
    for (auto& st : strips) st->refresh();
    velocity.setEnabled (proc.apvts.getRawParameterValue (PID::speed)->load() < 0.5f);
    const bool height = proc.apvts.getRawParameterValue (PID::pitchMode)->load() > 0.5f;
    chord.setVisible (! height);
    scale.setVisible (height);
    if (record.getToggleState() != proc.recorder.isRecording() && ticks % 5 == 0)
        record.setToggleState (proc.recorder.isRecording(), juce::dontSendNotification);
    if (proc.recorder.isRecording() || ticks % 15 == 0) take.repaint();

    if (++ticks % 10 == 0)
    {
        const auto idx = proc.input.deviceIndex();
        if (idx >= 0 && cameraBox.getSelectedItemIndex() != idx) cameraBox.setSelectedItemIndex (idx, juce::dontSendNotification);
        if (cameraBox.getNumItems() == 0 && ticks % 90 == 0) fillCameras();
        auto count = [] (int n, const char* one) { return n == 0 ? juce::String ("no ") + one + "s"
                                                        : n == 1 ? juce::String ("1 ") + one : juce::String (n) + " " + one + "s"; };
        const auto text = idx < 0 ? juce::String ("camera off")
                        : juce::String (juce::roundToInt (proc.input.fps.load())) + " fps   "
                              + count (proc.input.faceCount(), "face") + ", " + count (proc.input.handCount(), "hand");
        if (text != headerText)
        {
            headerText = text;
            repaint (0, 0, getWidth(), juce::roundToInt (46 * getWidth() / (float) kW));
        }
    }
}

} // namespace dy::mouth
