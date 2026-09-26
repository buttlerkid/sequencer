#include "PluginEditor.h"

namespace dy::nodal {

// Icons for the body selector, drawn into `r`.
static void drawBodyIcon (juce::Graphics& g, juce::Rectangle<float> r, int body, bool on)
{
    const auto c = r.getCentre();
    const float s = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path p;
    switch (static_cast<Body> (body))
    {
        case Body::Square:  p.addRectangle (c.x - s * 0.8f, c.y - s * 0.8f, s * 1.6f, s * 1.6f); break;
        case Body::Circle:  p.addEllipse (c.x - s * 0.85f, c.y - s * 0.85f, s * 1.7f, s * 1.7f); break;
        case Body::Hexagon: p.addPolygon (c, 6, s * 0.9f, juce::MathConstants<float>::halfPi); break;
        case Body::Violin:
        {
            const float w = s * 0.62f;
            p.startNewSubPath (c.x, c.y - s);
            p.cubicTo (c.x + w * 1.2f, c.y - s, c.x + w * 1.1f, c.y - s * 0.25f, c.x + w * 0.55f, c.y - s * 0.05f);
            p.cubicTo (c.x + w * 1.4f, c.y + s * 0.1f, c.x + w * 1.3f, c.y + s, c.x, c.y + s);
            p.cubicTo (c.x - w * 1.3f, c.y + s, c.x - w * 1.4f, c.y + s * 0.1f, c.x - w * 0.55f, c.y - s * 0.05f);
            p.cubicTo (c.x - w * 1.1f, c.y - s * 0.25f, c.x - w * 1.2f, c.y - s, c.x, c.y - s);
            p.closeSubPath();
            break;
        }
        case Body::Sphere:
            p.addEllipse (c.x - s * 0.85f, c.y - s * 0.85f, s * 1.7f, s * 1.7f);
            p.addEllipse (c.x - s * 0.85f, c.y - s * 0.3f, s * 1.7f, s * 0.6f);
            p.addEllipse (c.x - s * 0.3f, c.y - s * 0.85f, s * 0.6f, s * 1.7f);
            break;
        case Body::Cube:
            p.startNewSubPath (c.x, c.y - s * 0.9f);
            p.lineTo (c.x + s * 0.8f, c.y - s * 0.45f); p.lineTo (c.x + s * 0.8f, c.y + s * 0.45f); p.lineTo (c.x, c.y + s * 0.9f);
            p.lineTo (c.x - s * 0.8f, c.y + s * 0.45f); p.lineTo (c.x - s * 0.8f, c.y - s * 0.45f); p.closeSubPath();
            p.startNewSubPath (c.x - s * 0.8f, c.y - s * 0.45f); p.lineTo (c.x, c.y); p.lineTo (c.x + s * 0.8f, c.y - s * 0.45f);
            p.startNewSubPath (c.x, c.y); p.lineTo (c.x, c.y + s * 0.9f);
            break;
        case Body::Icosahedron:
            p.addPolygon (c, 6, s * 0.9f, 0.0f);
            p.startNewSubPath (c.x, c.y - s * 0.9f); p.lineTo (c.x - s * 0.45f, c.y + s * 0.15f); p.lineTo (c.x + s * 0.45f, c.y + s * 0.15f); p.closeSubPath();
            p.startNewSubPath (c.x - s * 0.45f, c.y + s * 0.15f); p.lineTo (c.x, c.y + s * 0.9f); p.lineTo (c.x + s * 0.45f, c.y + s * 0.15f);
            break;
        default: break;
    }
    g.setColour (on ? Colours::brass : Colours::text.withAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
}

static const char* tuneHelpText (int mode)
{
    switch (mode)
    {
        case TuneFree:     return "Modes keep the body's own inharmonic ratios: bells, gongs, glass.";
        case TuneScale:    return "Every mode is pulled to the nearest note of the key and scale.";
        default:           return "Every mode is pulled to a whole-number harmonic of the body pitch.";
    }
}

NodalEditor::NodalEditor (NodalProcessor& p)
    : AudioProcessorEditor (p), proc (p), renderer (p, viewState),
      pitch (p.apvts, PID::pitch, "Body size", true), glide (p.apvts, PID::glide, "Glide"), lock (p.apvts, PID::lock, "Lock"),
      density (p.apvts, PID::density, "Modes"), decay (p.apvts, PID::decay, "Decay"), damping (p.apvts, PID::damping, "Damping"),
      brightness (p.apvts, PID::brightness, "Bright"), spread (p.apvts, PID::spread, "Spread"), drive (p.apvts, PID::drive, "Drive"),
      lowCut (p.apvts, PID::lowCut, "Low cut"), highCut (p.apvts, PID::highCut, "High cut"), mix (p.apvts, PID::mix, "Mix"),
      output (p.apvts, PID::output, "Output"),
      key (p.apvts, PID::key, "Key"), scale (p.apvts, PID::scale, "Scale"), snap (p.apvts, PID::snap),
      tuneMode (p.apvts, PID::tuneMode),
      bodies (p.apvts, PID::body, { 0, 1, 2, 3 }, drawBodyIcon),
      materials (p.apvts, PID::material),
      plate (p, viewState), spectrum (p), meters (p), modPanel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* c : std::initializer_list<juce::Component*> { &tuningPanel, &resonancePanel, &ioPanel, &plate, &spectrum, &bodies, &materials,
                                                              &pitch, &glide, &lock, &density, &decay, &damping, &brightness, &spread, &drive,
                                                              &lowCut, &highCut, &mix, &output, &key, &scale, &snap, &tuneMode,
                                                              &viewSand, &viewLines, &viewField, &bodyInfo, &snapLabel, &tuneCaption, &tuneHelp, &meters,
                                                              &modPanel, &presetBox, &prevPreset, &nextPreset })
        content.addAndMakeVisible (c);

    bodyInfo.setJustificationType (juce::Justification::centred);
    bodyInfo.setFont (monoFont (11.5f));
    bodyInfo.setColour (juce::Label::textColourId, Colours::dim);
    snapLabel.setText ("Snap size to scale", juce::dontSendNotification);
    snapLabel.setFont (bodyFont (12.0f));
    tuneCaption.setText ("MODE TUNING", juce::dontSendNotification);
    tuneCaption.setFont (displayFont (10.5f));
    tuneCaption.setColour (juce::Label::textColourId, Colours::dim);
    tuneHelp.setFont (bodyFont (11.5f));
    tuneHelp.setColour (juce::Label::textColourId, Colours::dim);
    tuneHelp.setJustificationType (juce::Justification::topLeft);
    tuneMode.onChange = [this] (int m) { tuneHelp.setText (tuneHelpText (m), juce::dontSendNotification); lock.setEnabled (m != TuneFree); };
    tuneHelp.setText (tuneHelpText (static_cast<int> (std::lround (p.params.tuneMode->load()))), juce::dontSendNotification);

    {
        int id = 1;
        for (const auto& pr : factoryPresets()) presetBox.addItem (pr.name, id++);
        presetBox.setSelectedId (p.currentPreset + 1, juce::dontSendNotification);
        presetBox.setTooltip ("Factory presets");
        presetBox.onChange = [this]
        {
            const int idx = presetBox.getSelectedId() - 1;
            if (idx >= 0 && idx != proc.currentPreset) proc.loadPreset (idx);
        };
        prevPreset.onClick = [this] { stepPreset (-1); };
        nextPreset.onClick = [this] { stepPreset (1); };
    }

    int i = 0;
    for (auto* b : { &viewSand, &viewLines, &viewField })
    {
        b->setClickingTogglesState (false);
        b->setTooltip (i == 0 ? "Sand gathers on the nodal lines of whatever is ringing"
                      : i == 1 ? "Only the nodal lines, drawn from the ringing modes"
                               : "Vibration strength: bright where the plate moves most");
        b->onClick = [this, i] { setView (i); };
        ++i;
    }
    setView (p.uiView);

    viewState.grains = p.uiGrains;

    // UI test hook (standalone only): DY_NODAL_TEST_PRESET=<index> loads a factory preset when the window opens.
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("DY_NODAL_TEST_PRESET", {}); t.isNotEmpty())
        {
            p.loadPreset (t.getIntValue());
            presetBox.setSelectedId (p.currentPreset + 1, juce::dontSendNotification);
        }
    // DY_NODAL_TEST_STRIKE=<seconds> taps the plate at that interval.
    if (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        testStrikeTicks = juce::roundToInt (60.0 * juce::SystemStats::getEnvironmentVariable ("DY_NODAL_TEST_STRIKE", "0").getDoubleValue());

    gl.setPreferredVersion (juce::OpenGLVersion { 3, 2 });
    gl.setRenderer (this);
    gl.setComponentPaintingEnabled (true);
    gl.setContinuousRepainting (false);      // frames on demand, see timerCallback
    gl.attachTo (*this);

    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kW) / kH);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    startTimerHz (60);
}

NodalEditor::~NodalEditor()
{
    stopTimer();
    gl.detach();
    setLookAndFeel (nullptr);
}

void NodalEditor::setView (int v)
{
    viewState.view = v;
    proc.uiView = v;
    int i = 0;
    for (auto* b : { &viewSand, &viewLines, &viewField }) b->setToggleState (i++ == v, juce::dontSendNotification);
}

void NodalEditor::stepPreset (int delta)
{
    const int n = static_cast<int> (factoryPresets().size());
    proc.loadPreset (((proc.currentPreset + delta) % n + n) % n);
    presetBox.setSelectedId (proc.currentPreset + 1, juce::dontSendNotification);
}

void NodalEditor::paint (juce::Graphics& g)
{
    // Leave the plate area empty: the GL renderer draws there.
    g.saveState();
    g.excludeClipRegion (plateRect);
    g.fillAll (Colours::frame);
    g.setColour (Colours::line);
    const float s = getWidth() / static_cast<float> (kW);
    g.fillRect (0.0f, 46.0f * s, static_cast<float> (getWidth()), 1.0f);
    g.restoreState();

    g.addTransform (juce::AffineTransform::scale (s));
    g.setFont (displayFont (19.0f));
    g.setColour (Colours::text);
    g.drawText ("DY", 16, 0, 40, 46, juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.drawText ("NODAL", 48, 0, 100, 46, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (monoFont (11.0f));
    g.drawText ("v" DY_VERSION_STRING "  chladni resonator", 130, 0, 260, 46, juce::Justification::centredLeft);
    g.setFont (displayFont (10.0f));
    g.drawText ("PRESET", prevPreset.getX() - 70, 0, 62, 46, juce::Justification::centredRight);
}

void NodalEditor::resized()
{
    if (getWidth() <= 0) return;
    const float s = getWidth() / static_cast<float> (kW);
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);
    layout();

    plateRect = getLocalArea (&plate, plate.getLocalBounds());
    viewState.x = plateRect.getX();
    viewState.y = plateRect.getY();
    viewState.w = plateRect.getWidth();
    viewState.h = plateRect.getHeight();
    viewState.editorH = getHeight();
}

void NodalEditor::layout()
{
    const int top = 58, pad = 12, bottom = 728;

    // ---- header: presets
    {
        auto h = juce::Rectangle<int> (kW - pad - 330, 9, 330, 28);
        nextPreset.setBounds (h.removeFromRight (28));
        h.removeFromRight (4);
        auto pb = h.removeFromRight (240);
        presetBox.setBounds (pb);
        h.removeFromRight (4);
        prevPreset.setBounds (h.removeFromRight (28));
    }
    modPanel.setBounds (pad, bottom + 12, kW - 2 * pad, kH - 12 - (bottom + 12));

    // ---- left: tuning
    auto left = juce::Rectangle<int> (pad, top, 240, bottom - top);
    tuningPanel.setBounds (left);
    auto l = left.reduced (12, 10);
    l.removeFromTop (18);
    pitch.setBounds (l.removeFromTop (150).withSizeKeepingCentre (150, 150));
    bodyInfo.setBounds (l.removeFromTop (18));
    l.removeFromTop (10);
    {
        auto row = l.removeFromTop (40);
        key.setBounds (row.removeFromLeft (72));
        row.removeFromLeft (8);
        scale.setBounds (row);
    }
    l.removeFromTop (10);
    {
        auto row = l.removeFromTop (26);
        snap.setBounds (row.removeFromRight (56));
        snapLabel.setBounds (row);
    }
    l.removeFromTop (10);
    {
        auto row = l.removeFromTop (84);
        glide.setBounds (row.removeFromLeft (row.getWidth() / 2));
        lock.setBounds (row);
    }
    l.removeFromTop (12);
    tuneCaption.setBounds (l.removeFromTop (16));
    tuneMode.setBounds (l.removeFromTop (28));
    l.removeFromTop (6);
    tuneHelp.setBounds (l.removeFromTop (48));

    // ---- right: resonance + io
    auto right = juce::Rectangle<int> (kW - pad - 262, top, 262, bottom - top);
    auto rr = right;
    resonancePanel.setBounds (rr.removeFromTop (230));
    rr.removeFromTop (12);
    ioPanel.setBounds (rr);
    {
        auto r = resonancePanel.getBounds().reduced (10, 10);
        r.removeFromTop (20);
        const int kw = r.getWidth() / 3, kh = 96;
        auto row1 = r.removeFromTop (kh), row2 = r.removeFromTop (kh);
        density.setBounds (row1.removeFromLeft (kw)); decay.setBounds (row1.removeFromLeft (kw)); damping.setBounds (row1);
        brightness.setBounds (row2.removeFromLeft (kw)); spread.setBounds (row2.removeFromLeft (kw)); drive.setBounds (row2);
    }
    {
        auto r = ioPanel.getBounds().reduced (10, 10);
        r.removeFromTop (20);
        const int kw = r.getWidth() / 3, kh = 96;
        auto row1 = r.removeFromTop (kh), row2 = r.removeFromTop (kh);
        lowCut.setBounds (row1.removeFromLeft (kw)); highCut.setBounds (row1.removeFromLeft (kw)); mix.setBounds (row1);
        output.setBounds (row2.removeFromLeft (kw));
        meters.setBounds (row2.reduced (8, 24));
    }

    // ---- centre: plate, spectrum, selectors
    auto centre = juce::Rectangle<int> (left.getRight() + pad, top, right.getX() - pad - (left.getRight() + pad), bottom - top);
    auto c = centre;
    auto rowView = c.removeFromBottom (28);
    c.removeFromBottom (8);
    auto rowBodies = c.removeFromBottom (60);
    c.removeFromBottom (8);
    auto rowSpec = c.removeFromBottom (52);
    c.removeFromBottom (8);
    const int side = std::min (c.getWidth(), c.getHeight());
    plate.setBounds (c.withSizeKeepingCentre (side, side));
    spectrum.setBounds (rowSpec);
    bodies.setBounds (rowBodies);
    materials.setBounds (rowView.removeFromLeft (rowView.getWidth() - 200));
    rowView.removeFromLeft (10);
    const int vw = rowView.getWidth() / 3;
    viewSand.setBounds (rowView.removeFromLeft (vw));
    viewLines.setBounds (rowView.removeFromLeft (vw));
    viewField.setBounds (rowView);
}

void NodalEditor::timerCallback()
{
    // GL frames only while the plate is moving (or turning); a silent plate costs nothing.
    // Also redraw when the body / view changed, and at 4 Hz as a heartbeat.
    const int glKey = proc.telemetry.body.load() * 10 + viewState.view.load();
    if (viewState.busy.load() || glKey != lastGlKey || tick % 15 == 0) gl.triggerRepaint();
    lastGlKey = glKey;
    if (testStrikeTicks > 0 && tick % testStrikeTicks == 0) proc.requestStrike (0.9f);
    if (++tick % 2 != 0) return;              // UI widgets at 30 Hz
    meters.tick();
    (void) spectrum.tick();
    modPanel.tick();
    updateModRings();
    if (presetBox.getSelectedId() != proc.currentPreset + 1)
        presetBox.setSelectedId (proc.currentPreset + 1, juce::dontSendNotification);
    const float f0 = proc.telemetry.f0.load();
    const float cm = 30.0f * std::sqrt (261.63f / std::max (1.0f, f0));
    bodyInfo.setText (juce::String (f0, 1) + " Hz  " + juce::String::fromUTF8 ("\xE2\x89\x88 ") + juce::String (juce::roundToInt (cm)) + " cm plate",
                      juce::dontSendNotification);
}

} // namespace dy::nodal

namespace dy::nodal {

// Rings on the knobs that a modulation source is moving, in the source's colour.
void NodalEditor::updateModRings()
{
    const auto& pr = proc.params;
    const juce::Colour srcCol[3] = { modPanel.lfo1.accent, modPanel.lfo2.accent, modPanel.input.accent };
    const int srcTarget[3] = { static_cast<int> (std::lround (pr.lfoTarget[0]->load())), static_cast<int> (std::lround (pr.lfoTarget[1]->load())),
                               static_cast<int> (std::lround (pr.envTarget->load())) };
    auto colourFor = [&] (int t, bool extra, juce::Colour extraCol, bool& any)
    {
        int count = extra ? 1 : 0;
        juce::Colour c = extraCol;
        for (int s = 0; s < 3; ++s) if (srcTarget[s] == t) { ++count; c = srcCol[s]; }
        any = count > 0;
        return count > 1 ? Colours::text : c;
    };
    auto modOf = [&] (int t) { return proc.telemetry.mod[static_cast<size_t> (t)].load(); };
    auto plain = [] (std::atomic<float>* a) { return a->load(); };

    struct Row { int target; Knob* knob; };
    const Row rows[] = { { ModDecay, &decay }, { ModDamping, &damping }, { ModBrightness, &brightness }, { ModSpread, &spread },
                         { ModLock, &lock }, { ModMix, &mix }, { ModDrive, &drive } };
    for (const auto& row : rows)
    {
        bool any = false;
        const auto col = colourFor (row.target, false, {}, any);
        if (! any) { row.knob->clearModulation(); continue; }
        const float m = modOf (row.target);
        float v = 0.0f;
        switch (row.target)
        {
            case ModDecay:      v = plain (pr.decay) * std::pow (2.0f, m * 3.0f); break;
            case ModDamping:    v = plain (pr.damping) + m; break;
            case ModBrightness: v = plain (pr.brightness) + m; break;
            case ModSpread:     v = plain (pr.spread) + m; break;
            case ModLock:       v = plain (pr.lock) + m; break;
            case ModMix:        v = plain (pr.mix) + m; break;
            case ModDrive:      v = plain (pr.drive) + m * 24.0f; break;
            default: break;
        }
        row.knob->setModulation (v, col);
    }
    // body pitch: show where the body actually is (modulation, input pitch, scale snap)
    bool any = false;
    const auto col = colourFor (ModPitch, pr.trackPitch->load() > 0.5f, Colours::lilac, any);
    if (any) pitch.setModulation (proc.telemetry.note.load(), col);
    else     pitch.clearModulation();
}

} // namespace dy::nodal
