#include "PluginEditor.h"

namespace dy::dub {

using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

// ------------------------------------------------------------------ sources
const char* sourceName (int i)
{
    static const char* n[kNumSources] = { "LFO 1", "LFO 2", "LFO 3", "Follower" };
    return n[clampT (i, 0, kNumSources - 1)];
}

juce::Colour sourceColour (int i)
{
    static const juce::Colour c[kNumSources] = { Colours::teal, Colours::lilac, juce::Colour (0xffe58f8f), juce::Colour (0xff9fd18b) };
    return c[clampT (i, 0, kNumSources - 1)];
}

const juce::String& sourceTargetId (int i)
{
    static const juce::String ids[kNumSources] = { PID::lfo (0, "Target"), PID::lfo (1, "Target"), PID::lfo (2, "Target"), PID::folTarget };
    return ids[clampT (i, 0, kNumSources - 1)];
}

const juce::String& sourceAmountId (int i)
{
    static const juce::String ids[kNumSources] = { PID::lfo (0, "Amount"), PID::lfo (1, "Amount"), PID::lfo (2, "Amount"), PID::folAmount };
    return ids[clampT (i, 0, kNumSources - 1)];
}

static void setPlain (APVTS& s, const juce::String& id, float v)
{
    if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

// ------------------------------------------------------------------ small controls
Knob::Knob (APVTS& s, const juce::String& id, juce::String cap, int modTarget)
    : target (modTarget), state (s), caption (std::move (cap))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
    slider.setMouseDragSensitivity (180);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + (target != ModOff ? "  - right-click to modulate it with an LFO or the follower" : ""));
    }
    if (target != ModOff) slider.onRightClick = [this] { showMenu(); };
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (14)); }

void Knob::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.5f));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

void Knob::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Modulate " + caption);
    bool any = false;
    for (int i = 0; i < kNumSources; ++i)
    {
        const bool on = static_cast<int> (state.getRawParameterValue (sourceTargetId (i))->load()) == target;
        any = any || on;
        m.addItem (i + 1, sourceName (i), true, on);
    }
    m.addSeparator();
    m.addItem (100, "Remove modulation", any);
    juce::Component::SafePointer<Knob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& st = safe->state;
        if (r == 100)
        {
            for (int i = 0; i < kNumSources; ++i)
                if (static_cast<int> (st.getRawParameterValue (sourceTargetId (i))->load()) == safe->target)
                    setPlain (st, sourceTargetId (i), ModOff);
            return;
        }
        const int src = r - 1;
        setPlain (st, sourceTargetId (src), static_cast<float> (safe->target));
        if (std::abs (st.getRawParameterValue (sourceAmountId (src))->load()) < 0.01f) setPlain (st, sourceAmountId (src), 0.3f);
        if (safe->onAssigned) safe->onAssigned (src);
    });
}

void Knob::updateModulation (const EngineTelemetry& tel)
{
    if (target == ModOff) return;
    int src = -1;
    for (int i = 0; i < kNumSources && src < 0; ++i)
        if (static_cast<int> (state.getRawParameterValue (sourceTargetId (i))->load()) == target
            && std::abs (state.getRawParameterValue (sourceAmountId (i))->load()) > 0.001f) src = i;
    auto& props = slider.getProperties();
    if (src < 0)
    {
        if (shown > -2.0f) { props.remove ("modPos"); shown = -2.0f; slider.repaint(); }
        return;
    }
    const float v = tel.modNorm[static_cast<size_t> (target)].load();
    if (std::abs (v - shown) < 0.002f) return;
    shown = v;
    props.set ("modPos", v);
    props.set ("modColour", static_cast<juce::int64> (sourceColour (src).getARGB()));
    slider.repaint();
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

HSlider::HSlider (APVTS& s, const juce::String& id, juce::String cap) : caption (std::move (cap))
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
void HSlider::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (15)); }
void HSlider::paint (juce::Graphics& g)
{
    g.setColour (Colours::dim);
    g.setFont (bodyFont (11.0f));
    g.drawText (caption, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (Colours::text);
    g.setFont (monoFont (11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromTop (14), juce::Justification::centredRight);
}

ChoiceRow::ChoiceRow (APVTS& s, const juce::String& id, juce::StringArray l)
    : param (*s.getParameter (id)), labels (std::move (l)),
      attachment (*s.getParameter (id), [this] (float v) { current = juce::roundToInt (v); repaint(); }, nullptr)
{
    if (labels.isEmpty())
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&param)) labels = c->choices;
    attachment.sendInitialUpdate();
}

void ChoiceRow::paint (juce::Graphics& g)
{
    const float w = getWidth() / static_cast<float> (labels.size());
    for (int i = 0; i < labels.size(); ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0.0f, w, static_cast<float> (getHeight())).reduced (1.5f, 0.0f);
        const bool on = i == current;
        g.setColour (on ? Colours::brass : Colours::panel2);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? Colours::ink : Colours::text);
        g.setFont (bodyFont (11.0f, on));
        g.drawFittedText (labels[i], r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ChoiceRow::mouseDown (const juce::MouseEvent& e)
{
    const int i = clampT (static_cast<int> (e.position.x / (getWidth() / static_cast<float> (labels.size()))), 0, labels.size() - 1);
    attachment.setValueAsCompleteGesture (static_cast<float> (i));
}

void Scope::push()
{
    hist[static_cast<size_t> (head)] = tel.source[static_cast<size_t> (source)].load();
    head = (head + 1) % static_cast<int> (hist.size());
    repaint();
}

void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::scopeBg);
    g.fillRoundedRectangle (r, 5.0f);
    r = r.reduced (4.0f, 6.0f);
    const bool uni = source == kNumLfos;
    g.setColour (Colours::line);
    g.drawHorizontalLine (juce::roundToInt (uni ? r.getBottom() : r.getCentreY()), r.getX(), r.getRight());
    juce::Path p;
    const int n = static_cast<int> (hist.size());
    for (int i = 0; i < n; ++i)
    {
        const float v = hist[static_cast<size_t> ((head + i) % n)];
        const float y = uni ? r.getBottom() - v * r.getHeight() : r.getCentreY() - v * r.getHeight() * 0.5f;
        const float x = r.getX() + r.getWidth() * i / (n - 1);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (sourceColour (source));
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

SourcePage::SourcePage (DubProcessor& p, int src) : scope (p.telemetry(), src), source (src), state (p.apvts)
{
    addAndMakeVisible (scope);
    target = std::make_unique<Combo> (state, sourceTargetId (src), "Target");
    amount = std::make_unique<Knob> (state, sourceAmountId (src), "Amount");
    addAndMakeVisible (*target);
    addAndMakeVisible (*amount);
    if (src < kNumLfos)
    {
        shape = std::make_unique<Combo> (state, PID::lfo (src, "Shape"), "Shape");
        div = std::make_unique<Combo> (state, PID::lfo (src, "Div"), "Length");
        sync = std::make_unique<Toggle> (state, PID::lfo (src, "Sync"), "Sync to song");
        rate = std::make_unique<Knob> (state, PID::lfo (src, "Rate"), "Rate");
        for (juce::Component* c : { static_cast<juce::Component*> (shape.get()), static_cast<juce::Component*> (div.get()),
                                    static_cast<juce::Component*> (sync.get()), static_cast<juce::Component*> (rate.get()) })
            addAndMakeVisible (c);
    }
    else
    {
        attack = std::make_unique<Knob> (state, PID::folAttack, "Attack");
        release = std::make_unique<Knob> (state, PID::folRelease, "Release");
        addAndMakeVisible (*attack);
        addAndMakeVisible (*release);
        amount->slider.setTooltip ("Positive: the target opens as the input gets louder. Negative: it ducks");
    }
}

void SourcePage::resized()
{
    auto r = getLocalBounds();
    auto col1 = r.removeFromLeft (150);
    if (shape != nullptr)
    {
        shape->setBounds (col1.removeFromTop (40));
        col1.removeFromTop (8);
    }
    target->setBounds (col1.removeFromTop (40));
    r.removeFromLeft (10);
    if (sync != nullptr)
    {
        auto col2 = r.removeFromLeft (112);
        sync->setBounds (col2.removeFromTop (26).withTrimmedTop (2));
        col2.removeFromTop (14);
        div->setBounds (col2.removeFromTop (40));
        r.removeFromLeft (6);
        rate->setBounds (r.removeFromLeft (66).withHeight (88));
    }
    else
    {
        attack->setBounds (r.removeFromLeft (66).withHeight (88));
        release->setBounds (r.removeFromLeft (66).withHeight (88));
    }
    amount->setBounds (r.removeFromLeft (66).withHeight (88));
    r.removeFromLeft (8);
    scope.setBounds (r.withHeight (88));
}

void SourcePage::tick()
{
    if (sync != nullptr)
    {
        const bool s = sync->getToggleState();
        div->setEnabled (s);
        rate->setEnabled (! s);
        rate->setAlpha (s ? 0.4f : 1.0f);
        div->setAlpha (s ? 1.0f : 0.4f);
    }
    scope.push();
}

// ------------------------------------------------------------------ editor
DubEditor::DubEditor (DubProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      rate (p.apvts, PID::rate, "Seq rate"), pattern (p.apvts, PID::pattern, "Pattern"), run (p.apvts, PID::run, "Run"),
      length (p.apvts, PID::length, "Steps"), swing (p.apvts, PID::swing, "Swing"),
      grid (p), env (p),
      filterMode (p.apvts, PID::filterMode, { "LP", "BP", "HP" }), revType (p.apvts, PID::revType),
      cutoff (p.apvts, PID::cutoff, "Cutoff", ModCutoff), res (p.apvts, PID::res, "Res", ModRes), envDepth (p.apvts, PID::envDepth, "Env depth", ModEnvDepth),
      dlyTime (p.apvts, PID::dlyTime, ""), pingPong (p.apvts, PID::pingPong, "Ping-pong"),
      dlyFb (p.apvts, PID::dlyFb, "Feedback", ModDelayFb), dlyHp (p.apvts, PID::dlyHp, "High pass"), dlyLp (p.apvts, PID::dlyLp, "Tone"),
      dlyLevel (p.apvts, PID::dlyLevel, "Level", ModDelayLevel),
      revSize (p.apvts, PID::revSize, "Size", ModRevSize), revDecay (p.apvts, PID::revDecay, "Decay"), revTone (p.apvts, PID::revTone, "Tone"),
      revLevel (p.apvts, PID::revLevel, "Level", ModRevLevel),
      pan (p.apvts, PID::pan, "Pan", ModPan), level (p.apvts, PID::level, "Level", ModLevel), mix (p.apvts, PID::mix, "Mix", ModMix),
      output (p.apvts, PID::output, "Output")
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    preset.addItemList (presetNames(), 1);
    preset.setSelectedItemIndex (p.currentPreset, juce::dontSendNotification);
    preset.onChange = [this] { const int i = preset.getSelectedItemIndex(); if (i >= 0 && i != proc.currentPreset) { proc.setCurrentProgram (i); grid.repaint(); env.repaint(); } };
    prevPreset.onClick = [this] { preset.setSelectedItemIndex ((proc.currentPreset + preset.getNumItems() - 1) % preset.getNumItems()); };
    nextPreset.onClick = [this] { preset.setSelectedItemIndex ((proc.currentPreset + 1) % preset.getNumItems()); };

    for (auto* b : { &bankA, &bankB })
    {
        b->setClickingTogglesState (false);
        b->setTooltip ("Show this bank's steps (the pattern decides which one plays)");
    }
    bankA.onClick = [this] { setBank (0); };
    bankB.onClick = [this] { setBank (1); };
    copyBank.setTooltip ("Copy every step of the bank on view to the other bank");
    copyBank.onClick = [this]
    {
        for (int s = 0; s < kSteps; ++s) proc.steps.setStep (1 - viewBank, s, proc.steps.getStep (viewBank, s));
    };
    grid.onSelect = [this] (int s) { env.step = s; env.repaint(); };
    rate.box.setTooltip ("How long one step is");
    pattern.box.setTooltip ("Which bank plays on each pass through the steps: AAB plays A twice, then B");
    dlyTime.box.setTooltip ("Delay time, locked to the tempo");
    run.box.setTooltip ("Always: keeps sequencing on its own clock while the song is stopped");

    for (auto* c : std::initializer_list<juce::Component*> { &preset, &prevPreset, &nextPreset, &bankA, &bankB, &copyBank, &rate, &pattern, &run,
                                                              &length, &swing, &grid, &env, &filterMode, &revType, &cutoff, &res, &envDepth,
                                                              &dlyTime, &pingPong, &dlyFb, &dlyHp, &dlyLp, &dlyLevel, &revSize, &revDecay, &revTone,
                                                              &revLevel, &pan, &level, &mix, &output })
        content.addAndMakeVisible (c);

    for (int i = 0; i < kNumSources; ++i)
    {
        auto& t = srcTabs[static_cast<size_t> (i)];
        t.setButtonText (sourceName (i));
        t.setColour (juce::TextButton::buttonOnColourId, sourceColour (i));
        t.setColour (juce::TextButton::textColourOnId, Colours::ink);
        t.onClick = [this, i] { setSource (i); };
        content.addAndMakeVisible (t);
        pages.push_back (std::make_unique<SourcePage> (p, i));
        content.addChildComponent (*pages.back());
    }
    for (auto* k : { &cutoff, &res, &envDepth, &dlyFb, &dlyLevel, &revSize, &revLevel, &pan, &level, &mix })
        k->onAssigned = [this] (int s) { setSource (s); };

    setBank (0);
    setSource (0);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kW) / kH);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    startTimerHz (30);
}

DubEditor::~DubEditor() { stopTimer(); setLookAndFeel (nullptr); }

void DubEditor::setBank (int b)
{
    viewBank = b;
    grid.bank = env.bank = b;
    bankA.setToggleState (b == 0, juce::dontSendNotification);
    bankB.setToggleState (b == 1, juce::dontSendNotification);
    copyBank.setButtonText (b == 0 ? "Copy A > B" : "Copy B > A");
    grid.repaint();
    env.repaint();
}

void DubEditor::setSource (int s)
{
    viewSource = s;
    for (int i = 0; i < kNumSources; ++i)
    {
        srcTabs[static_cast<size_t> (i)].setToggleState (i == s, juce::dontSendNotification);
        pages[static_cast<size_t> (i)]->setVisible (i == s);
    }
}

void DubEditor::panel (juce::Graphics& g, juce::Rectangle<int> r, const char* title) const
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    if (title != nullptr)
    {
        g.setColour (Colours::dim);
        g.setFont (displayFont (10.5f));
        g.drawText (title, r.getX() + 12, r.getY() + 8, 200, 14, juce::Justification::centredLeft);
    }
}

void DubEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::frame);
    const float s = getWidth() / static_cast<float> (kW);
    g.addTransform (juce::AffineTransform::scale (s));
    g.setColour (Colours::line);
    g.fillRect (0, 46, kW, 1);
    g.setFont (displayFont (19.0f));
    g.setColour (Colours::text);
    g.drawText ("DY", 16, 0, 40, 46, juce::Justification::centredLeft);
    g.setColour (Colours::brass);
    g.drawText ("DUB", 48, 0, 100, 46, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (monoFont (11.0f));
    g.drawText ("v" DY_VERSION_STRING "  step sequencer effect", 100, 0, 240, 46, juce::Justification::centredLeft);

    const auto& tel = proc.telemetry();
    const bool synced = proc.hostSynced.load(), running = tel.running.load();
    g.setColour (synced ? Colours::teal : Colours::dim);
    juce::String status = juce::String (proc.shownBpm.load(), 1) + " bpm   " + (synced ? "synced to the song" : running ? "own clock" : "waiting for play");
    if (running && tel.step.load() >= 0)
        status << "   bank " << (tel.bank.load() == 0 ? "A" : "B") << " step " << (tel.step.load() + 1);
    g.drawText (status, kW - 420, 0, 404, 46, juce::Justification::centredRight);

    panel (g, rSeq, nullptr);
    panel (g, rGrid, nullptr);
    panel (g, rEnv, nullptr);
    panel (g, rFilter, "FILTER");
    panel (g, rDelay, "DELAY");
    panel (g, rReverb, "REVERB");
    panel (g, rMod, "MODULATION");
    panel (g, rOut, "OUTPUT");

    // which bank is playing: a dot beside its button
    if (running && tel.step.load() >= 0)
    {
        auto& b = tel.bank.load() == 0 ? bankA : bankB;
        g.setColour (Colours::brass);
        g.fillEllipse (static_cast<float> (b.getRight()) - 9.0f, static_cast<float> (b.getY()) + 3.0f, 6.0f, 6.0f);
    }
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.5f));
    g.drawText ("BANK", rSeq.getX() + 12, rSeq.getY() + 6, 60, 14, juce::Justification::centredLeft);

    // in / out meters in the output panel
    auto m = juce::Rectangle<int> (rOut.getX() + 14, rOut.getBottom() - 30, rOut.getWidth() - 28, 18);
    const float pk[2] = { tel.inPeak.load(), tel.outPeak.load() };
    for (int i = 0; i < 2; ++i)
    {
        auto row = m.removeFromTop (8).toFloat();
        m.removeFromTop (2);
        g.setColour (Colours::scopeBg);
        g.fillRoundedRectangle (row.withTrimmedLeft (28.0f), 3.0f);
        const float db = pk[i] > 1e-5f ? 20.0f * std::log10 (pk[i]) : -100.0f;
        const float t = clampT ((db + 48.0f) / 54.0f, 0.0f, 1.0f);
        g.setColour (db > -0.5f ? juce::Colour (0xffe58f8f) : Colours::brass);
        g.fillRoundedRectangle (row.withTrimmedLeft (28.0f).withWidth ((row.getWidth() - 28.0f) * t), 3.0f);
        g.setColour (Colours::dim);
        g.setFont (monoFont (9.0f));
        g.drawText (i == 0 ? "IN" : "OUT", row.withWidth (26.0f).expanded (0.0f, 2.0f), juce::Justification::centredLeft);
    }
}

void DubEditor::resized()
{
    const float s = getWidth() / static_cast<float> (kW);
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    const int pad = 12;
    preset.setBounds (440, 10, 230, 26);
    prevPreset.setBounds (406, 10, 30, 26);
    nextPreset.setBounds (674, 10, 30, 26);

    rSeq = { pad, 56, kW - 2 * pad, 58 };
    {
        auto r = rSeq.reduced (12, 8);
        auto bankArea = r.removeFromLeft (220).withTrimmedTop (14);
        bankA.setBounds (bankArea.removeFromLeft (48).withHeight (26));
        bankArea.removeFromLeft (6);
        bankB.setBounds (bankArea.removeFromLeft (48).withHeight (26));
        bankArea.removeFromLeft (10);
        copyBank.setBounds (bankArea.withHeight (26));
        r.removeFromLeft (20);
        rate.setBounds (r.removeFromLeft (110)); r.removeFromLeft (12);
        pattern.setBounds (r.removeFromLeft (130)); r.removeFromLeft (12);
        length.setBounds (r.removeFromLeft (200)); r.removeFromLeft (16);
        swing.setBounds (r.removeFromLeft (200)); r.removeFromLeft (16);
        run.setBounds (r.removeFromLeft (150));
    }

    rGrid = { pad, rSeq.getBottom() + 10, kW - 2 * pad, 356 };
    grid.setBounds (rGrid.reduced (10, 9));

    const int lowY = rGrid.getBottom() + 10, lowH = kH - lowY - pad;
    rEnv = { pad, lowY, 360, lowH };
    env.setBounds (rEnv.reduced (6, 4));

    const int x0 = rEnv.getRight() + 10, w = kW - pad - x0;
    const int h1 = 156, h2 = lowH - h1 - 10;
    rFilter = { x0, lowY, 214, h1 };
    rDelay  = { rFilter.getRight() + 10, lowY, 290, h1 };
    rReverb = { rDelay.getRight() + 10, lowY, x0 + w - rDelay.getRight() - 10, h1 };
    rMod    = { x0, lowY + h1 + 10, 520, h2 };
    rOut    = { rMod.getRight() + 10, rMod.getY(), x0 + w - rMod.getRight() - 10, h2 };

    auto inner = [] (juce::Rectangle<int> r) { r = r.reduced (10, 8); r.removeFromTop (20); return r; };
    auto knobs = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> ks)
    {
        const int kw = r.getWidth() / static_cast<int> (ks.size());
        for (auto* k : ks) k->setBounds (r.removeFromLeft (kw).withHeight (88));
    };
    {
        auto r = inner (rFilter);
        filterMode.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
        knobs (r, { &cutoff, &res, &envDepth });
    }
    {
        auto r = inner (rDelay);
        auto top = r.removeFromTop (22);
        pingPong.setBounds (top.removeFromRight (90));
        dlyTime.setBounds (top.removeFromLeft (120).withTop (top.getY() - 15));
        r.removeFromTop (6);
        knobs (r, { &dlyFb, &dlyHp, &dlyLp, &dlyLevel });
    }
    {
        auto r = inner (rReverb);
        revType.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
        knobs (r, { &revSize, &revDecay, &revTone, &revLevel });
    }
    {
        auto r = rMod.reduced (10, 8);
        auto tabs = r.removeFromTop (24).withTrimmedLeft (110);
        for (auto& t : srcTabs) { t.setBounds (tabs.removeFromLeft (84)); tabs.removeFromLeft (6); }
        r.removeFromTop (10);
        for (auto& pg : pages) pg->setBounds (r);
    }
    {
        auto r = inner (rOut);
        r.removeFromBottom (30);
        knobs (r, { &pan, &level, &mix, &output });
    }
}

void DubEditor::timerCallback()
{
    const auto& tel = proc.telemetry();
    const int v = proc.steps.version.load();
    if (v != shownVersion || tel.running.load()) { shownVersion = v; grid.repaint(); env.repaint(); }
    if (proc.currentPreset != shownPreset)
    {
        shownPreset = proc.currentPreset;
        preset.setSelectedItemIndex (shownPreset, juce::dontSendNotification);
    }
    for (auto* k : { &cutoff, &res, &envDepth, &dlyFb, &dlyLevel, &revSize, &revLevel, &pan, &level, &mix })
        k->updateModulation (tel);
    pages[static_cast<size_t> (viewSource)]->tick();
    const float s = getWidth() / static_cast<float> (kW);
    repaint (0, 0, getWidth(), juce::roundToInt (46 * s));
    repaint (juce::Rectangle<int> (rSeq.getX(), rSeq.getY(), 240, rSeq.getHeight()).transformedBy (juce::AffineTransform::scale (s)));
    repaint (rOut.transformedBy (juce::AffineTransform::scale (s)).withTrimmedTop (juce::roundToInt ((rOut.getHeight() - 34) * s)));
}

} // namespace dy::dub
