#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Scale.h"

namespace dy {

DYSequencerProcessor::DYSequencerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    params.bind (apvts);
    eventScratch.reserve (1024);
    heldAuditions.reserve (64);

    // A friendly starting point: four-on-the-floor on track 1, pattern A.
    for (int i = 0; i < 16; i += 4)
        patterns[0].tracks[0].setActive (i, true);
}

int DYSequencerProcessor::targetPattern() const
{
    return juce::jlimit (0, kNumPatterns - 1, static_cast<int> (std::lround (params.pattern->load())));
}

void DYSequencerProcessor::selectPattern (int i)
{
    if (auto* p = apvts.getParameter (ParamIDs::pattern))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (juce::jlimit (0, kNumPatterns - 1, i))));
        p->endChangeGesture();
    }
}

// ------------------------------------------------------------------ settings
bool DYSequencerProcessor::anyTrackSoloed() const
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto& t = params.tracks[static_cast<size_t> (i)];
        if (t.enabled->load() > 0.5f && t.solo->load() > 0.5f)
            return true;
    }
    return false;
}

TrackSettingsArray DYSequencerProcessor::allTrackSettings() const
{
    TrackSettingsArray ts;
    const bool solo = anyTrackSoloed();
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto s = trackSettings (i);
        if (solo && ! s.solo)
            s.mute = true;
        ts[static_cast<size_t> (i)] = s;
    }
    return ts;
}

void DYSequencerProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sequencer.reset();
    internalPpq = 0.0;
    heldAuditions.clear();
}

bool DYSequencerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

// ------------------------------------------------------------------ audio
void DYSequencerProcessor::runSequencer (double ppqStart, int numSamples, int sampleOffset, bool playing,
                                         const GlobalSettings& g, const TrackSettingsArray& ts, const PatternModel& pattern,
                                         PatternSwitch sw, juce::MidiBuffer& midi)
{
    Transport t { playing, ppqStart, numSamples };
    sequencer.process (t, g, ts, pattern, eventScratch, sw);

    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
    for (const auto& e : eventScratch)
    {
        int off = playing ? static_cast<int> (std::llround ((e.ppq - ppqStart) / ppqPerSample)) : 0;
        off = juce::jlimit (0, juce::jmax (0, numSamples - 1), off) + sampleOffset;

        midi.addEvent (e.noteOn ? juce::MidiMessage::noteOn (e.channel, e.note, static_cast<juce::uint8> (e.velocity))
                                : juce::MidiMessage::noteOff (e.channel, e.note),
                       off);
        if (e.noteOn)
            hitCount[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, e.track))].fetch_add (1, std::memory_order_relaxed);
    }
}

void DYSequencerProcessor::runAuditions (int numSamples, juce::MidiBuffer& midi)
{
    // Release notes whose time is up.
    for (auto it = heldAuditions.begin(); it != heldAuditions.end();)
    {
        if (it->samplesLeft < numSamples)
        {
            midi.addEvent (juce::MidiMessage::noteOff (it->channel, it->note), juce::jmax (0, it->samplesLeft));
            it = heldAuditions.erase (it);
        }
        else
        {
            it->samplesLeft -= numSamples;
            ++it;
        }
    }

    // Start the ones the editor queued.
    const int holdSamples = static_cast<int> (currentSampleRate * 0.18);
    int start1, size1, start2, size2;
    auditionFifo.prepareToRead (auditionFifo.getNumReady(), start1, size1, start2, size2);
    auto take = [&] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& a = auditionSlots[static_cast<size_t> (start + i)];
            for (auto it = heldAuditions.begin(); it != heldAuditions.end();)
            {
                if (it->channel == a.channel && it->note == a.note)
                {
                    midi.addEvent (juce::MidiMessage::noteOff (a.channel, a.note), 0);
                    it = heldAuditions.erase (it);
                }
                else
                    ++it;
            }
            midi.addEvent (juce::MidiMessage::noteOn (a.channel, a.note, static_cast<juce::uint8> (a.velocity)), 0);
            heldAuditions.push_back ({ a.channel, a.note, holdSamples });
        }
    };
    take (start1, size1);
    take (start2, size2);
    auditionFifo.finishedRead (size1 + size2);
}

void DYSequencerProcessor::readIncomingMidi (juce::MidiBuffer& midi)
{
    const bool follow = params.midiFollow->load() > 0.5f;
    if (! follow) return;                 // pass incoming MIDI straight through

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            midiTranspose.store (juce::jlimit (-24, 24, m.getNoteNumber() - 60), std::memory_order_relaxed);
    }
    midi.clear();                         // the keyboard steers the sequencer, it does not play through
}

void DYSequencerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();                       // we make MIDI, not audio

    const int numSamples = buffer.getNumSamples();
    readIncomingMidi (midi);

    GlobalSettings g = params.readGlobal();
    g.sampleRate = currentSampleRate;
    g.midiTranspose = params.midiFollow->load() > 0.5f ? midiTranspose.load (std::memory_order_relaxed) : 0;

    double bpm = 120.0, ppq = 0.0, loopStart = 0.0, loopEnd = 0.0;
    bool playing = false, haveHostPpq = false, looping = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          bpm = *b;
            if (auto p = pos->getPpqPosition())  { ppq = *p; haveHostPpq = true; }
            playing = pos->getIsPlaying();
            if (auto lp = pos->getLoopPoints())
            {
                looping   = pos->getIsLooping();
                loopStart = lp->ppqStart;
                loopEnd   = lp->ppqEnd;
            }
        }
    }

    if (! haveHostPpq)
    {
        // No transport (standalone app): free-run so the sequencer is still usable.
        playing = true;
        ppq     = internalPpq;
    }

    g.bpm = bpm > 1.0 ? bpm : 120.0;
    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
    const auto   ts           = allTrackSettings();

    // Pattern change: immediate when stopped, otherwise at the next bar line.
    const int target  = targetPattern();
    int       current = currentPattern.load (std::memory_order_relaxed);
    if (target != current && ! playing)
    {
        currentPattern.store (target, std::memory_order_relaxed);
        current = target;
    }
    const bool pending = target != current;
    const auto& cur  = patterns[static_cast<size_t> (current)];
    const auto& next = patterns[static_cast<size_t> (target)];

    const double ppqEnd = ppq + numSamples * ppqPerSample;
    bool switched = false;

    if (playing && looping && loopEnd > loopStart && ppq < loopEnd && ppqEnd > loopEnd + 1e-9)
    {
        // Hosts that do not split buffers at the loop point: play up to the loop end,
        // then continue from the loop start inside the same buffer.
        const int firstLen = juce::jlimit (0, numSamples, static_cast<int> (std::floor ((loopEnd - ppq) / ppqPerSample)));
        const double switchAt = nextBarAtOrAfter (ppq);
        PatternSwitch sw1 = pending ? PatternSwitch { &next, switchAt } : PatternSwitch {};
        runSequencer (ppq, firstLen, 0, true, g, ts, cur, sw1, midi);

        switched = pending && switchAt <= loopEnd + 1e-9;
        const double switchAt2 = nextBarAtOrAfter (loopStart);
        PatternSwitch sw2 = (pending && ! switched) ? PatternSwitch { &next, switchAt2 } : PatternSwitch {};
        runSequencer (loopStart, numSamples - firstLen, firstLen, true, g, ts, switched ? next : cur, sw2, midi);
        if (pending && ! switched)
            switched = switchAt2 <= loopStart + (numSamples - firstLen) * ppqPerSample + 1e-9;
    }
    else
    {
        const double switchAt = nextBarAtOrAfter (ppq);
        PatternSwitch sw = (pending && playing) ? PatternSwitch { &next, switchAt } : PatternSwitch {};
        runSequencer (ppq, numSamples, 0, playing, g, ts, cur, sw, midi);
        switched = pending && playing && switchAt <= ppqEnd + 1e-9;
    }

    if (switched)
        currentPattern.store (target, std::memory_order_relaxed);

    runAuditions (numSamples, midi);

    if (! haveHostPpq)
        internalPpq += numSamples * ppqPerSample;

    uiBpm.store (g.bpm);
    uiPpq.store (ppq);
    uiPlaying.store (playing);
    uiInternalClock.store (! haveHostPpq);
}

// ------------------------------------------------------------------ tracks
juce::String DYSequencerProcessor::trackName (int i) const
{
    const auto& n = trackNames[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, i))];
    return n.isNotEmpty() ? n : "Track " + juce::String (i + 1);
}

void DYSequencerProcessor::setTrackName (int i, const juce::String& name)
{
    trackNames[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, i))] = name.trim().substring (0, 24);
}

int DYSequencerProcessor::trackBaseNote (int i) const
{
    const auto s = trackSettings (i);
    if (s.pitchMode == PitchFixed)
        return juce::jlimit (0, 127, s.fixedNote);
    const auto g = params.readGlobal();
    return noteForDegree (g.key, g.scale, kBaseNote, s.transpose);
}

juce::String DYSequencerProcessor::trackNoteName (int i) const
{
    return juce::MidiMessage::getMidiNoteName (trackBaseNote (i), true, true, 3);
}

int DYSequencerProcessor::numEnabledTracks() const
{
    int n = 0;
    for (int i = 0; i < kNumTracks; ++i) if (isTrackEnabled (i)) ++n;
    return n;
}

void DYSequencerProcessor::setBoolParam (int i, const char* suffix, bool value)
{
    if (auto* p = apvts.getParameter (ParamIDs::track (i, suffix)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (value ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

int DYSequencerProcessor::addTrack()
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        if (! isTrackEnabled (i))
        {
            setBoolParam (i, ParamIDs::enabled, true);
            return i;
        }
    }
    return -1;
}

void DYSequencerProcessor::removeTrack (int i)
{
    setBoolParam (i, ParamIDs::enabled, false);
    setBoolParam (i, ParamIDs::solo, false);
}

void DYSequencerProcessor::applyPadLayout (PadLayout layout)
{
    struct Pad { int note; const char* name; };
    static const Pad gm[kNumTracks] = {
        { 36, "Kick" },   { 38, "Snare" },   { 42, "HH Closed" }, { 46, "HH Open" },
        { 39, "Clap" },   { 37, "Rim" },     { 41, "Tom Lo" },    { 45, "Tom Mid" },
        { 48, "Tom Hi" }, { 49, "Crash" },   { 51, "Ride" },      { 56, "Cowbell" },
        { 54, "Tamb" },   { 70, "Shaker" },  { 75, "Clave" },     { 44, "HH Pedal" },
    };

    for (int i = 0; i < kNumTracks; ++i)
    {
        auto* pitch = apvts.getParameter (ParamIDs::track (i, ParamIDs::pitchMode));
        auto* note  = apvts.getParameter (ParamIDs::track (i, ParamIDs::fixedNote));
        if (pitch == nullptr || note == nullptr) continue;

        auto set = [] (juce::RangedAudioParameter* p, float plain)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        };

        switch (layout)
        {
            case PadLayout::GmDrums:
                set (pitch, static_cast<float> (PitchFixed));
                set (note, static_cast<float> (gm[i].note));
                trackNames[static_cast<size_t> (i)] = gm[i].name;
                break;
            case PadLayout::ChromaticC1:
                set (pitch, static_cast<float> (PitchFixed));
                set (note, static_cast<float> (36 + i));
                break;
            case PadLayout::Melodic:
                set (pitch, static_cast<float> (PitchScale));
                break;
        }
    }
}

void DYSequencerProcessor::clearAll()
{
    for (auto& t : editPattern().tracks)
        t.clear();
}

void DYSequencerProcessor::audition (int i)
{
    const auto s = trackSettings (i);
    int start1, size1, start2, size2;
    auditionFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        auditionSlots[static_cast<size_t> (start1)] = { juce::jlimit (1, 16, s.channel), trackBaseNote (i),
                                                        juce::jlimit (1, 127, 100 + s.velOffset) };
        auditionFifo.finishedWrite (1);
    }
}

// ------------------------------------------------------------------ state
static juce::String intsToString (const std::vector<int>& v)
{
    juce::String s;
    s.preallocateBytes (static_cast<size_t> (v.size()) * 4);
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0) s += ' ';
        s += juce::String (v[i]);
    }
    return s;
}

static std::vector<int> stringToInts (const juce::String& s)
{
    std::vector<int> v;
    for (const auto& tok : juce::StringArray::fromTokens (s, " ", ""))
        if (tok.isNotEmpty()) v.push_back (tok.getIntValue());
    return v;
}

void DYSequencerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DYSequencer");
    root.setProperty ("version", 3, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);

    juce::ValueTree names ("Names");
    for (int i = 0; i < kNumTracks; ++i)
        if (trackNames[static_cast<size_t> (i)].isNotEmpty())
            names.setProperty ("t" + juce::String (i), trackNames[static_cast<size_t> (i)], nullptr);
    root.addChild (names, -1, nullptr);

    for (int pi = 0; pi < kNumPatterns; ++pi)
    {
        juce::ValueTree pat ("Pattern");
        pat.setProperty ("index", pi, nullptr);
        for (int i = 0; i < kNumTracks; ++i)
        {
            const auto& tm = patterns[static_cast<size_t> (pi)].tracks[i];

            // Skip untouched tracks to keep the state small.
            bool any = false;
            for (int s = 0; s < kMaxSteps && ! any; ++s) any = tm.isActive (s);
            for (int l = 0; l < static_cast<int> (Lane::Count) && ! any; ++l)
                for (int s = 0; s < kMaxSteps && ! any; ++s)
                    any = tm.get (static_cast<Lane> (l), s) != laneInfo (static_cast<Lane> (l)).def;
            if (! any) continue;

            juce::ValueTree tr ("Track");
            tr.setProperty ("index", i, nullptr);

            std::vector<int> act (kMaxSteps);
            for (int s = 0; s < kMaxSteps; ++s) act[static_cast<size_t> (s)] = tm.isActive (s) ? 1 : 0;
            tr.setProperty ("active", intsToString (act), nullptr);

            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
            {
                std::vector<int> vals (kMaxSteps);
                for (int s = 0; s < kMaxSteps; ++s) vals[static_cast<size_t> (s)] = tm.get (static_cast<Lane> (l), s);
                tr.setProperty (laneInfo (static_cast<Lane> (l)).name, intsToString (vals), nullptr);
            }
            pat.addChild (tr, -1, nullptr);
        }
        root.addChild (pat, -1, nullptr);
    }

    juce::ValueTree ui ("UI");
    ui.setProperty ("theme", uiTheme, nullptr);
    ui.setProperty ("scale", uiScale, nullptr);
    ui.setProperty ("exportBars", uiExportBars, nullptr);
    ui.setProperty ("selectedTrack", uiSelectedTrack, nullptr);
    root.addChild (ui, -1, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void DYSequencerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.isValid()) return;

    auto paramsTree = root.getChildWithName (apvts.state.getType());
    if (paramsTree.isValid())
        apvts.replaceState (paramsTree);

    for (auto& p : patterns)
        for (auto& t : p.tracks)
            t.clear();
    for (auto& n : trackNames) n.clear();

    auto names = root.getChildWithName ("Names");
    if (names.isValid())
        for (int i = 0; i < kNumTracks; ++i)
            trackNames[static_cast<size_t> (i)] = names.getProperty ("t" + juce::String (i), "").toString();

    // v3 saves one <Pattern index=n> per bank; v2 saved a single <Pattern> (bank A)
    // that also carried the track names.
    for (const auto& pat : root)
    {
        if (! pat.hasType ("Pattern")) continue;
        const int pi = juce::jlimit (0, kNumPatterns - 1, static_cast<int> (pat.getProperty ("index", 0)));

        for (const auto& tr : pat)
        {
            const int i = tr.getProperty ("index", -1);
            if (i < 0 || i >= kNumTracks) continue;
            auto& tm = patterns[static_cast<size_t> (pi)].tracks[i];

            if (tr.hasProperty ("name"))
                trackNames[static_cast<size_t> (i)] = tr.getProperty ("name").toString();

            const auto act = stringToInts (tr.getProperty ("active").toString());
            for (size_t s = 0; s < act.size() && s < kMaxSteps; ++s) tm.setActive (static_cast<int> (s), act[s] != 0);

            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
            {
                const auto lane = static_cast<Lane> (l);
                if (! tr.hasProperty (laneInfo (lane).name)) continue;
                const auto vals = stringToInts (tr.getProperty (laneInfo (lane).name).toString());
                for (size_t s = 0; s < vals.size() && s < kMaxSteps; ++s) tm.set (lane, static_cast<int> (s), vals[s]);
            }
        }
    }
    currentPattern.store (targetPattern(), std::memory_order_relaxed);

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
    {
        uiTheme         = ui.getProperty ("theme", 0);
        uiScale         = static_cast<float> (static_cast<double> (ui.getProperty ("scale", 1.0)));
        uiExportBars    = ui.getProperty ("exportBars", 4);
        uiSelectedTrack = juce::jlimit (0, kNumTracks - 1, static_cast<int> (ui.getProperty ("selectedTrack", 0)));
    }
}

// ------------------------------------------------------------------ clipboard
TrackClip DYSequencerProcessor::makeTrackClip (int i) const
{
    TrackClip c;
    c.steps = editPattern().tracks[i].snapshot();
    c.name  = trackNames[static_cast<size_t> (i)];
    for (auto* suffix : trackParamSuffixes())
        if (auto* p = apvts.getParameter (ParamIDs::track (i, suffix)))
            c.params.push_back (p->getValue());
    return c;
}

void DYSequencerProcessor::applyTrackClip (int i, const TrackClip& clip)
{
    editPattern().tracks[i].load (clip.steps);
    trackNames[static_cast<size_t> (i)] = clip.name;

    size_t k = 0;
    for (auto* suffix : trackParamSuffixes())
    {
        // Routing stays with the destination track; only musical content is pasted.
        if (! isRoutingParam (suffix) && k < clip.params.size())
        {
            if (auto* p = apvts.getParameter (ParamIDs::track (i, suffix)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (clip.params[k]);
                p->endChangeGesture();
            }
        }
        ++k;
    }
}

void DYSequencerProcessor::copyTrack (int i)  { trackClip = makeTrackClip (i); }
void DYSequencerProcessor::pasteTrack (int i) { if (trackClip) applyTrackClip (i, *trackClip); }
void DYSequencerProcessor::clearTrack (int i) { editPattern().tracks[i].clear(); }

void DYSequencerProcessor::copyPattern()
{
    std::array<TrackClip, kNumTracks> all;
    for (int i = 0; i < kNumTracks; ++i) all[static_cast<size_t> (i)] = makeTrackClip (i);
    patternClip = std::move (all);
}

void DYSequencerProcessor::pastePattern()
{
    if (! patternClip) return;
    for (int i = 0; i < kNumTracks; ++i) applyTrackClip (i, (*patternClip)[static_cast<size_t> (i)]);
}

// ------------------------------------------------------------------ export
juce::File DYSequencerProcessor::exportMidiFile (int bars)
{
    GlobalSettings g = params.readGlobal();
    g.bpm        = uiBpm.load();
    g.sampleRate = currentSampleRate;
    const auto ts = allTrackSettings();

    const auto events = Sequencer::renderOffline (g, ts, editPattern(), bars);

    juce::MidiFile mf;
    const int tpq = 960;
    mf.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (60000000.0 / g.bpm)), 0.0);
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    tempo.addEvent (juce::MidiMessage::endOfTrack(), bars * 4.0 * tpq);
    mf.addTrack (tempo);

    for (int i = 0; i < kNumTracks; ++i)
    {
        if (! ts[static_cast<size_t> (i)].enabled) continue;

        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, trackName (i)), 0.0);
        for (const auto& e : events)
        {
            if (e.track != i) continue;
            auto m = e.noteOn ? juce::MidiMessage::noteOn (e.channel, e.note, static_cast<juce::uint8> (e.velocity))
                              : juce::MidiMessage::noteOff (e.channel, e.note);
            seq.addEvent (m, e.ppq * tpq);
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), bars * 4.0 * tpq);
        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DY Sequencer Pattern.mid");
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (os.openedOk())
    {
        mf.writeTo (os, 1);
        os.flush();
    }
    return file;
}

juce::AudioProcessorEditor* DYSequencerProcessor::createEditor()
{
    return new DYSequencerEditor (*this);
}

} // namespace dy

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dy::DYSequencerProcessor();
}
