#include "PluginEditor.h"

namespace dy::dub {

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
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + (target != ModOff ? "  - right-click to modulate it with an LFO or the follower" : ""));
    }
    if (target != ModOff) slider.onRightClick = [this] { showMenu(); };
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (16)); }

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
    else if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (s.getParameter (id)))
        for (int v = ip->getRange().getStart(); v <= ip->getRange().getEnd(); ++v) box.addItem (juce::String (v), v - ip->getRange().getStart() + 1);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
}
void Combo::resized() { box.setBounds (getLocalBounds().withTrimmedTop (caption.isEmpty() ? 0 : 16)); }
void Combo::paint (juce::Graphics& g)
{
    if (caption.isEmpty()) return;
    g.setColour (Colours::dim);
    g.setFont (displayFont (9.0f));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

Toggle::Toggle (APVTS& s, const juce::String& id, juce::String text)
{
    setButtonText (text);
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawInset (g, r, 8.0f);
    if (title.isNotEmpty())
    {
        g.setColour (Colours::bright);
        g.setFont (displayFont (12.5f));
        g.drawText (title, r.removeFromTop (28.0f), juce::Justification::centred);
    }
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
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r, 5.0f);
    r = r.reduced (4.0f, 8.0f);
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
    g.strokePath (p, juce::PathStrokeType (1.6f));
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
        sync = std::make_unique<Toggle> (state, PID::lfo (src, "Sync"), "Sync");
        rate = std::make_unique<Knob> (state, PID::lfo (src, "Rate"), "Rate");
        for (juce::Component* c : { static_cast<juce::Component*> (shape.get()), static_cast<juce::Component*> (div.get()),
                                    static_cast<juce::Component*> (sync.get()), static_cast<juce::Component*> (rate.get()) })
            addAndMakeVisible (c);
        sync->setTooltip ("Locked to the song (length in bars) or free running (rate in Hz)");
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
    if (shape != nullptr) { shape->setBounds (col1.removeFromTop (44)); col1.removeFromTop (10); }
    target->setBounds (col1.removeFromTop (44));
    r.removeFromLeft (14);
    if (sync != nullptr)
    {
        auto col2 = r.removeFromLeft (110);
        sync->setBounds (col2.removeFromTop (28).withTrimmedTop (2));
        col2.removeFromTop (8);
        div->setBounds (col2.removeFromTop (44));
        r.removeFromLeft (8);
        rate->setBounds (r.removeFromLeft (76).withHeight (100));
    }
    else
    {
        attack->setBounds (r.removeFromLeft (76).withHeight (100));
        release->setBounds (r.removeFromLeft (76).withHeight (100));
    }
    amount->setBounds (r.removeFromLeft (76).withHeight (100));
    r.removeFromLeft (10);
    scope.setBounds (r.withHeight (100));
}

void SourcePage::tick()
{
    if (sync != nullptr)
    {
        const bool s = sync->getToggleState();
        div->setEnabled (s);
        rate->setEnabled (! s);
        div->setAlpha (s ? 1.0f : 0.4f);
    }
    scope.push();
}

void ModSummary::paint (juce::Graphics& g)
{
    const auto& tel = proc.telemetry();
    const float rowH = getHeight() / static_cast<float> (kNumSources);
    for (int i = 0; i < kNumSources; ++i)
    {
        auto r = juce::Rectangle<float> (0.0f, i * rowH, static_cast<float> (getWidth()), rowH).reduced (2.0f, 3.0f);
        const int t = static_cast<int> (proc.apvts.getRawParameterValue (sourceTargetId (i))->load());
        const float amt = proc.apvts.getRawParameterValue (sourceAmountId (i))->load();
        const bool active = t != ModOff && std::abs (amt) > 0.001f;
        drawRaised (g, r, 4.0f);
        const auto col = sourceColour (i);
        g.setColour (active ? col : col.withAlpha (0.3f));
        g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setFont (displayFont (10.0f));
        g.setColour (active ? Colours::text : Colours::dim);
        g.drawText (juce::String (sourceName (i)).toUpperCase(), r.withTrimmedLeft (26.0f).withWidth (80.0f), juce::Justification::centredLeft);
        g.setFont (bodyFont (12.5f));
        g.drawText (t == ModOff ? juce::String ("not assigned") : juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + targetDef (t).name,
                    r.withTrimmedLeft (106.0f).withWidth (140.0f), juce::Justification::centredLeft);
        if (t != ModOff)
        {
            g.setFont (monoFont (11.5f));
            g.setColour (Colours::brass);
            g.drawText ((amt > 0 ? "+" : "") + juce::String (juce::roundToInt (amt * 100.0f)) + "%", r.withTrimmedLeft (246.0f).withWidth (56.0f),
                        juce::Justification::centredRight);
        }
        // the live value
        auto bar = juce::Rectangle<float> (r.getRight() - 96.0f, r.getCentreY() - 3.0f, 84.0f, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (bar, 3.0f);
        const float v = tel.source[static_cast<size_t> (i)].load();
        g.setColour (col.withAlpha (active ? 0.9f : 0.35f));
        if (i == kNumLfos) g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * clampT (v, 0.0f, 1.0f)), 3.0f);
        else
        {
            const float cx = bar.getCentreX(), x = cx + v * bar.getWidth() * 0.5f;
            g.fillRoundedRectangle (juce::Rectangle<float> (std::min (cx, x), bar.getY(), std::abs (x - cx) + 1.0f, bar.getHeight()), 2.0f);
        }
    }
}

void ModSummary::mouseDown (const juce::MouseEvent& e)
{
    if (onPick) onPick (clampT (static_cast<int> (e.position.y / (getHeight() / static_cast<float> (kNumSources))), 0, kNumSources - 1));
}

// ------------------------------------------------------------------ editor
DubEditor::DubEditor (DubProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      pattern (p.apvts, PID::pattern, "Pattern"),
      strips (p),
      cutoff (p.apvts, PID::cutoff, "Cutoff", ModCutoff), res (p.apvts, PID::res, "Resonance", ModRes),
      level (p.apvts, PID::level, "Volume", ModLevel), swing (p.apvts, PID::swing, "Swing"), output (p.apvts, PID::output, "Output"),
      glide (p.apvts, PID::glide, "Glide"), offset (p.apvts, PID::offset, "Offset"),
      mix (p.apvts, PID::mix, "Dry/Wet", ModMix), pan (p.apvts, PID::pan, "Pan", ModPan),
      envDepth2 (p.apvts, PID::envDepth, "Env depth", ModEnvDepth),
      dlyLevel (p.apvts, PID::dlyLevel, "Mix", ModDelayLevel), dlyFb (p.apvts, PID::dlyFb, "Feedback", ModDelayFb),
      dlyLp (p.apvts, PID::dlyLp, "Tone"), dlyHp (p.apvts, PID::dlyHp, "HP Freq"), pingPong (p.apvts, PID::pingPong, "Ping-pong"),
      revLevel (p.apvts, PID::revLevel, "Mix", ModRevLevel), revSize (p.apvts, PID::revSize, "Size", ModRevSize),
      revTone (p.apvts, PID::revTone, "Damp"), revWidth (p.apvts, PID::revWidth, "Width"), revDecay (p.apvts, PID::revDecay, "Decay"),
      revPre (p.apvts, PID::revPre, "Pre-dly"),
      filterMode (p.apvts, PID::filterMode, "Filter"), rate (p.apvts, PID::rate, "Seq rate"), length (p.apvts, PID::length, "Steps"),
      run (p.apvts, PID::run, "Run"), dlyTime (p.apvts, PID::dlyTime, "Dly rate"), revType (p.apvts, PID::revType, "Reverb"),
      env (p), envDepth (p.apvts, PID::envDepth, "Depth", ModEnvDepth),
      summary (p), grid (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // header
    category.addItem ("All", 1);
    category.addItem ("Factory", 2);
    category.addItem ("User", 3);
    category.setSelectedId (1, juce::dontSendNotification);
    category.onChange = [this] { refreshPresets(); };
    preset.setTextWhenNothingSelected ("Preset");
    preset.onChange = [this]
    {
        const int id = preset.getSelectedId();
        if (id >= 1000 && id - 1000 < userFiles.size()) loadUserPreset (proc, userFiles[id - 1000]);
        else if (id > 0 && id - 1 != proc.currentPreset) proc.setCurrentProgram (id - 1);
        else return;
        shownName = proc.presetName;
        strips.tick();
    };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    save.onClick = [this] { savePreset(); };
    save.setTooltip ("Save this sound as a user preset");
    random.onClick = [this]
    {
        randomiseSteps (proc.steps, viewBank, RandomAll);
        proc.currentPreset = -1;
        proc.presetName = "Random";
        showPresetName();
    };
    random.setTooltip ("Roll a new pattern for the bank on view: every lane, sends and envelopes");
    for (auto* b : { &bankA, &bankB }) b->setTooltip ("Show this bank's steps (the pattern decides which one plays)");
    bankA.onClick = [this] { setBank (0); };
    bankB.onClick = [this] { setBank (1); };
    pattern.box.setTooltip ("Which bank plays on each pass through the steps: AAB plays A twice, then B");

    // strips and the row under them
    strips.onSelect = [this] (int s) { env.step = s; grid.selected = s; env.repaint(); grid.repaint(); };
    rndCut.onClick = [this] { randomiseSteps (proc.steps, viewBank, RandomCut); };
    rndRes.onClick = [this] { randomiseSteps (proc.steps, viewBank, RandomRes); };
    rndVol.onClick = [this] { randomiseSteps (proc.steps, viewBank, RandomVol); };
    rndOn.onClick  = [this] { randomiseSteps (proc.steps, viewBank, RandomOn); };
    copyBank.onClick = [this] { for (int s = 0; s < kSteps; ++s) proc.steps.setStep (1 - viewBank, s, proc.steps.getStep (viewBank, s)); };
    copyBank.setTooltip ("Copy every step of the bank on view to the other bank");
    static const char* tabNames[kNumPages] = { "Adv", "Main", "Env", "Mod" };
    static const char* tabTips[kNumPages] = { "Pan, chance, filter envelope depth and sends, per step", "Sequencer, delay and reverb",
                                              "Volume and filter envelopes of the selected step", "LFOs and the input follower" };
    for (int i = 0; i < kNumPages; ++i)
    {
        auto& t = tabs[static_cast<size_t> (i)];
        t.setButtonText (tabNames[i]);
        t.setTooltip (tabTips[i]);
        t.getProperties().set ("tab", true);
        t.onClick = [this, i] { setPage (i); };
    }

    for (auto* c : std::initializer_list<juce::Component*> { &category, &preset, &prevPreset, &nextPreset, &save, &random, &bankA, &bankB, &pattern,
                                                              &strips, &rndCut, &rndRes, &rndVol, &rndOn, &copyBank })
        content.addAndMakeVisible (c);
    for (auto& t : tabs) content.addAndMakeVisible (t);
    for (auto& pg : pages) content.addChildComponent (pg);

    // MAIN
    auto& main = pages[PageMain];
    for (auto* s : { &secSeq, &secDelay, &secReverb }) main.addAndMakeVisible (s);
    for (auto* k : { &cutoff, &res, &level, &swing, &output, &glide, &offset, &mix, &pan, &envDepth2 }) secSeq.addAndMakeVisible (k);
    for (auto* k : { &dlyLevel, &dlyFb, &dlyLp, &dlyHp, &pingPong }) secDelay.addAndMakeVisible (k);
    for (auto* k : { &revLevel, &revSize, &revTone, &revWidth, &revDecay, &revPre }) secReverb.addAndMakeVisible (k);
    for (auto* c : { &filterMode, &rate, &length, &run, &dlyTime, &revType }) main.addAndMakeVisible (c);
    cutoff.slider.setTooltip ("Moves every step's cutoff up or down (octaves) - right-click to modulate");
    glide.slider.setTooltip ("Step values slide into each other instead of jumping");
    offset.slider.setTooltip ("Moves the steps later or earlier against the beat");
    pingPong.slider.setTooltip ("0 %: a stereo echo. 100 %: the repeats bounce left - right");
    run.box.setTooltip ("Always: keeps sequencing on its own clock while the song is stopped");

    // ENV
    auto& envPage = pages[PageEnv];
    envPage.addAndMakeVisible (secEnv);
    envPage.addAndMakeVisible (secEnvDepth);
    secEnv.addAndMakeVisible (env);
    secEnvDepth.addAndMakeVisible (envDepth);
    envDepth.slider.setTooltip ("Scales every step's filter envelope depth (the F.ENV lane on the ADV page)");

    // MOD
    auto& modPage = pages[PageMod];
    modPage.addAndMakeVisible (secMod);
    modPage.addAndMakeVisible (secRouting);
    for (int i = 0; i < kNumSources; ++i)
    {
        auto& t = srcTabs[static_cast<size_t> (i)];
        t.setButtonText (sourceName (i));
        t.getProperties().set ("accent", static_cast<juce::int64> (sourceColour (i).getARGB()));
        t.onClick = [this, i] { setSource (i); };
        secMod.addAndMakeVisible (t);
        srcPages.push_back (std::make_unique<SourcePage> (p, i));
        secMod.addChildComponent (*srcPages.back());
    }
    secRouting.addAndMakeVisible (summary);
    summary.onPick = [this] (int s) { setSource (s); };
    for (auto* k : { &cutoff, &res, &envDepth, &envDepth2, &dlyFb, &dlyLevel, &revSize, &revLevel, &pan, &level, &mix })
        k->onAssigned = [this] (int s) { setSource (s); setPage (PageMod); };

    // ADV: the per-step lanes that are not on the strips, in the strips' columns
    pages[PageAdv].addAndMakeVisible (secAdv);
    secAdv.addAndMakeVisible (grid);
    grid.onSelect = [this] (int s) { strips.select (s); };

    refreshPresets();
    setBank (0);
    setSource (0);
    // UI tests: DY_DUB_PAGE=0..3 opens ADV / MAIN / ENV / MOD
    setPage (juce::jlimit (0, kNumPages - 1, juce::SystemStats::getEnvironmentVariable ("DY_DUB_PAGE", "1").getIntValue()));
    sendLookAndFeelChange();          // the controls were made before the look was set: rebuild their value boxes
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kW) / kH);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
    startTimerHz (30);
}

DubEditor::~DubEditor() { stopTimer(); setLookAndFeel (nullptr); }

void DubEditor::refreshPresets()
{
    const int cat = category.getSelectedId();
    preset.clear (juce::dontSendNotification);
    if (cat != 3)
    {
        if (cat == 1) preset.addSectionHeading ("Factory");
        auto names = presetNames();
        for (int i = 0; i < names.size(); ++i) preset.addItem (names[i], i + 1);
    }
    userFiles = userPresets();
    if (cat != 2 && ! userFiles.isEmpty())
    {
        if (cat == 1) preset.addSectionHeading ("User");
        for (int i = 0; i < userFiles.size(); ++i) preset.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
    }
    showPresetName();
}

void DubEditor::showPresetName()
{
    shownName = proc.presetName;
    if (proc.currentPreset >= 0 && preset.indexOfItemId (proc.currentPreset + 1) >= 0)
    {
        preset.setSelectedId (proc.currentPreset + 1, juce::dontSendNotification);
        return;
    }
    for (int i = 0; i < userFiles.size(); ++i)
        if (proc.currentPreset < 0 && userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (proc.presetName))
        {
            preset.setSelectedId (1000 + i, juce::dontSendNotification);
            return;
        }
    preset.setText (proc.presetName, juce::dontSendNotification);
}

void DubEditor::stepPreset (int delta)
{
    const int n = preset.getNumItems();
    if (n == 0) return;
    int i = preset.getSelectedItemIndex();
    i = i < 0 ? (delta > 0 ? 0 : n - 1) : (i + delta + n) % n;
    preset.setSelectedItemIndex (i, juce::sendNotificationSync);
}

void DubEditor::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for this sound:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", proc.presetName == "Random" ? juce::String() : proc.presetName);
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DubEditor> safe (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name");
        safe->saveDialog->setVisible (false);
        if (r == 1 && name.trim().isNotEmpty())
        {
            if (! saveUserPreset (safe->proc, name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", "Could not write to\n" + userPresetFolder().getFullPathName());
            safe->category.setSelectedId (1, juce::dontSendNotification);
            safe->refreshPresets();
        }
    }), false);
}

void DubEditor::setBank (int b)
{
    viewBank = b;
    strips.setBank (b);
    grid.bank = env.bank = b;
    bankA.setToggleState (b == 0, juce::dontSendNotification);
    bankB.setToggleState (b == 1, juce::dontSendNotification);
    copyBank.setButtonText (b == 0 ? "Copy A > B" : "Copy B > A");
    grid.repaint();
    env.repaint();
}

void DubEditor::setPage (int pg)
{
    page = pg;
    for (int i = 0; i < kNumPages; ++i)
    {
        tabs[static_cast<size_t> (i)].setToggleState (i == pg, juce::dontSendNotification);
        pages[static_cast<size_t> (i)].setVisible (i == pg);
    }
}

void DubEditor::setSource (int s)
{
    viewSource = s;
    for (int i = 0; i < kNumSources; ++i)
    {
        srcTabs[static_cast<size_t> (i)].setToggleState (i == s, juce::dontSendNotification);
        srcPages[static_cast<size_t> (i)]->setVisible (i == s);
    }
}

void DubEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::page);
    const float s = getWidth() / static_cast<float> (kW);
    g.addTransform (juce::AffineTransform::scale (s));

    // the body: a brushed charcoal slab
    juce::ColourGradient slab (Colours::frameTop, 0.0f, 0.0f, Colours::frame.darker (0.15f), 0.0f, static_cast<float> (kH), false);
    g.setGradientFill (slab);
    g.fillRect (0, 0, kW, kH);
    for (int y = 0; y < kH; y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha ((y / 2) % 3 == 0 ? 0.008f : 0.0f));
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (kW));
    }

    // header
    drawLogo (g, { 16.0f, 11.0f, 36.0f, 36.0f });
    g.setFont (displayFont (22.0f));
    g.setColour (Colours::bright);
    g.drawText ("DY DUB", 62, 4, 180, 34, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (displayFont (8.5f));
    g.drawText ("STEP SEQUENCER FX  v" DY_VERSION_STRING, 64, 36, 220, 12, juce::Justification::centredLeft);

    const auto& tel = proc.telemetry();
    const bool synced = proc.hostSynced.load(), running = tel.running.load();
    auto st = juce::Rectangle<int> (kW - 150, 10, 136, 40);
    drawInset (g, st.toFloat(), 4.0f);
    g.setFont (monoFont (13.0f));
    g.setColour (Colours::brass);
    g.drawText (juce::String (proc.shownBpm.load(), 1) + " BPM", st.withHeight (22).translated (0, 2), juce::Justification::centred);
    g.setFont (displayFont (8.5f));
    g.setColour (synced ? Colours::teal : Colours::dim);
    g.drawText (synced ? "SYNCED TO SONG" : running ? "OWN CLOCK" : "WAITING FOR PLAY", st.withTrimmedTop (22).withHeight (14), juce::Justification::centred);
}

void DubEditor::resized()
{
    const float s = getWidth() / static_cast<float> (kW);
    proc.uiScale = s;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    const int pad = 12;
    // header: preset browser, bank keys, pattern
    category.setBounds (300, 16, 92, 28);
    prevPreset.setBounds (398, 16, 28, 28);
    preset.setBounds (430, 16, 290, 28);
    nextPreset.setBounds (724, 16, 28, 28);
    save.setBounds (764, 16, 70, 28);
    random.setBounds (840, 16, 88, 28);
    bankA.setBounds (950, 16, 32, 28);
    bankB.setBounds (986, 16, 32, 28);
    pattern.setBounds (1032, 0, 120, 44);

    strips.setBounds (pad, 62, kW - 2 * pad, 410);

    const int rowY = strips.getBottom() + 8;
    int x = pad;
    for (auto* b : { &rndCut, &rndRes, &rndVol, &rndOn }) { b->setBounds (x, rowY, 128, 30); x += 134; }
    copyBank.setBounds (x + 6, rowY, 120, 30);
    int tx = kW - pad - kNumPages * 112;
    for (auto& t : tabs) { t.setBounds (tx, rowY, 108, 30); tx += 112; }

    const auto pageArea = juce::Rectangle<int> (pad, rowY + 38, kW - 2 * pad, kH - rowY - 38 - pad);
    for (auto& pg : pages) pg.setBounds (pageArea);
    const int W = pageArea.getWidth(), H = pageArea.getHeight();

    auto gridOf = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> ks, int cols)
    {
        const int kw = r.getWidth() / cols, rows = (static_cast<int> (ks.size()) + cols - 1) / cols, kh = r.getHeight() / rows;
        int i = 0;
        for (auto* k : ks) { k->setBounds (r.getX() + (i % cols) * kw, r.getY() + (i / cols) * kh, kw, kh); ++i; }
    };
    {
        // MAIN: three sections, then the drop-downs
        const int secH = H - 58;
        secSeq.setBounds (0, 0, 640, secH);
        secDelay.setBounds (secSeq.getRight() + 10, 0, 300, secH);
        secReverb.setBounds (secDelay.getRight() + 10, 0, W - secDelay.getRight() - 10, secH);
        auto inner = [] (const Section& sc) { return sc.getLocalBounds().reduced (14, 6).withTrimmedTop (24); };
        gridOf (inner (secSeq), { &cutoff, &res, &level, &swing, &output, &glide, &offset, &mix, &pan, &envDepth2 }, 5);
        gridOf (inner (secDelay), { &dlyLevel, &dlyFb, &dlyLp, &dlyHp, &pingPong }, 3);
        gridOf (inner (secReverb), { &revLevel, &revSize, &revTone, &revWidth, &revDecay, &revPre }, 3);
        auto row = juce::Rectangle<int> (0, secH + 10, W, 46);
        const int cw = (W - 5 * 12) / 6;
        for (auto* c : { &filterMode, &rate, &length, &run, &dlyTime, &revType }) { c->setBounds (row.removeFromLeft (cw)); row.removeFromLeft (12); }
    }
    {
        // ENV
        secEnv.setBounds (0, 0, W - 190, H);
        secEnvDepth.setBounds (W - 180, 0, 180, H);
        env.setBounds (secEnv.getLocalBounds().reduced (10, 6));
        envDepth.setBounds (secEnvDepth.getLocalBounds().reduced (20, 0).withTrimmedTop (40).withHeight (110));
    }
    {
        // MOD
        secMod.setBounds (0, 0, 760, H);
        secRouting.setBounds (770, 0, W - 770, H);
        auto r = secMod.getLocalBounds().reduced (16, 8).withTrimmedTop (26);
        auto tabRow = r.removeFromTop (28);
        for (auto& t : srcTabs) { t.setBounds (tabRow.removeFromLeft (96)); tabRow.removeFromLeft (6); }
        r.removeFromTop (16);
        for (auto& sp : srcPages) sp->setBounds (r);
        summary.setBounds (secRouting.getLocalBounds().reduced (14, 8).withTrimmedTop (28));
    }
    {
        // ADV: same columns as the strips above
        secAdv.setBounds (0, 0, W, H);
        grid.setBounds (0, 8, W, H - 12);
        const int toggleH = 34, barH = (grid.getHeight() - 24 - 6 * 3 - 2 * toggleH - 4) / 3;
        grid.configure ({ 4, 5, 6, 7, 8 }, StripArea::kGutter, barH, toggleH);
    }
}

void DubEditor::timerCallback()
{
    const auto& tel = proc.telemetry();
    strips.tick();
    const int v = proc.steps.version.load();
    const bool changed = v != shownVersion;
    shownVersion = v;
    if (page == PageAdv && (changed || tel.running.load())) grid.repaint();
    if (page == PageEnv && (changed || tel.running.load())) env.repaint();
    if (page == PageMod) { srcPages[static_cast<size_t> (viewSource)]->tick(); summary.repaint(); }
    if (proc.presetName != shownName) showPresetName();
    for (auto* k : { &cutoff, &res, &envDepth, &envDepth2, &dlyFb, &dlyLevel, &revSize, &revLevel, &pan, &level, &mix })
        k->updateModulation (tel);
    const float s = getWidth() / static_cast<float> (kW);
    repaint (juce::Rectangle<int> (kW - 160, 0, 160, 58).transformedBy (juce::AffineTransform::scale (s)));
}

} // namespace dy::dub
