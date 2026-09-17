#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Scale.h"
#include <algorithm>

namespace dy {

DYSequencerProcessor::DYSequencerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    params.bind (apvts);
    eventScratch.reserve (1024);
    heldAuditions.reserve (64);
    heldNotes.reserve (16);

    // A friendly starting point: four-on-the-floor on track 1, pattern A; chain = A x4.
    for (int i = 0; i < 16; i += 4)
        patterns[0].tracks[0].setActive (i, true);
    chainPatterns[0] = 0;
    chainBars[0] = 4;

    // Length / Euclid parameters are a window onto the edited pattern's settings.
    static const char* const fields[] = { ParamIDs::steps, ParamIDs::pulses, ParamIDs::rotate, ParamIDs::euclidMode };
    for (int i = 0; i < kNumTracks; ++i)
        for (int f = 0; f < 4; ++f)
        {
            const auto id = ParamIDs::track (i, fields[f]);
            settingParamIds[id] = { i, f };
            apvts.addParameterListener (id, this);
        }
    apvts.addParameterListener (ParamIDs::pattern, this);

    startTimerHz (30);
}

DYSequencerProcessor::~DYSequencerProcessor()
{
    stopTimer();
    for (const auto& kv : settingParamIds)
        apvts.removeParameterListener (kv.first, this);
    apvts.removeParameterListener (ParamIDs::pattern, this);
}

// ------------------------------------------------------------------ pattern / settings sync
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

void DYSequencerProcessor::parameterChanged (const juce::String& id, float newValue)
{
    if (id == ParamIDs::pattern)
    {
        settingsDirty.store (true);
        return;
    }
    if (syncingParams) return;

    const auto it = settingParamIds.find (id);
    if (it == settingParamIds.end()) return;

    auto& tm = editPattern().tracks[it->second.track];
    const int v = static_cast<int> (std::lround (newValue));
    switch (it->second.field)
    {
        case 0: tm.setSteps (v); break;
        case 1: tm.setPulses (v); break;
        case 2: tm.setRotate (v); break;
        case 3: tm.setEuclidMode (v); break;
        default: break;
    }
}

void DYSequencerProcessor::pushPatternSettingsToParams()
{
    syncingParams = true;
    static const char* const fields[] = { ParamIDs::steps, ParamIDs::pulses, ParamIDs::rotate, ParamIDs::euclidMode };
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto& tm = editPattern().tracks[i];
        const int values[] = { tm.steps(), tm.pulses(), tm.rotate(), tm.euclidMode() };
        for (int f = 0; f < 4; ++f)
        {
            if (auto* p = apvts.getParameter (ParamIDs::track (i, fields[f])))
            {
                const float norm = p->convertTo0to1 (static_cast<float> (values[f]));
                if (std::abs (p->getValue() - norm) > 1e-6f)
                    p->setValueNotifyingHost (norm);
            }
        }
    }
    syncingParams = false;
}

void DYSequencerProcessor::copyParamsToAllPatternSettings()
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto& t = params.tracks[static_cast<size_t> (i)];
        for (auto& p : patterns)
        {
            auto& tm = p.tracks[i];
            tm.setSteps (static_cast<int> (std::lround (t.steps->load())));
            tm.setPulses (static_cast<int> (std::lround (t.pulses->load())));
            tm.setRotate (static_cast<int> (std::lround (t.rotate->load())));
            tm.setEuclidMode (static_cast<int> (std::lround (t.euclidMode->load())));
        }
    }
}

void DYSequencerProcessor::timerCallback()
{
    if (settingsDirty.exchange (false))
        pushPatternSettingsToParams();
}

// ------------------------------------------------------------------ chain
ChainEntry DYSequencerProcessor::chainEntry (int i) const
{
    i = juce::jlimit (0, kMaxChainEntries - 1, i);
    return { chainPatterns[static_cast<size_t> (i)].load(), chainBars[static_cast<size_t> (i)].load() };
}

void DYSequencerProcessor::setChainEntry (int i, ChainEntry e)
{
    if (i < 0 || i >= kMaxChainEntries) return;
    chainPatterns[static_cast<size_t> (i)].store (juce::jlimit (0, kNumPatterns - 1, e.pattern));
    chainBars[static_cast<size_t> (i)].store (juce::jlimit (1, kMaxChainEntryBars, e.bars));
}

void DYSequencerProcessor::addChainEntry()
{
    const int n = chainLength.load();
    if (n >= kMaxChainEntries) return;
    const auto last = chainEntry (juce::jmax (0, n - 1));
    setChainEntry (n, { n > 0 ? (last.pattern + 1) % kNumPatterns : 0, last.bars });
    chainLength.store (n + 1);
}

void DYSequencerProcessor::removeLastChainEntry()
{
    const int n = chainLength.load();
    if (n > 1) chainLength.store (n - 1);
}

int DYSequencerProcessor::chainTotalBars() const
{
    int total = 0;
    for (int i = 0; i < chainLength.load(); ++i) total += chainEntry (i).bars;
    return total;
}

int DYSequencerProcessor::expandChain()
{
    int total = 0;
    const int n = chainLength.load (std::memory_order_relaxed);
    for (int i = 0; i < n && total < kMaxChainBars; ++i)
    {
        const auto e = chainEntry (i);
        for (int b = 0; b < e.bars && total < kMaxChainBars; ++b)
            barMap[static_cast<size_t> (total++)] = &patterns[static_cast<size_t> (e.pattern)];
    }
    return total;
}

void DYSequencerProcessor::chainPosition (int& entry, int& barInEntry) const
{
    entry = -1; barInEntry = 0;
    const int total = chainTotalBars();
    if (total <= 0) return;
    int bar = posMod (static_cast<int64_t> (std::floor (uiPpq.load() / 4.0 + 1e-9)), total);
    for (int i = 0; i < chainLength.load(); ++i)
    {
        const int bars = chainEntry (i).bars;
        if (bar < bars) { entry = i; barInEntry = bar; return; }
        bar -= bars;
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
                                         const GlobalSettings& g, const TrackSettingsArray& ts,
                                         const PatternSchedule& sched, juce::MidiBuffer& midi)
{
    Transport t { playing, ppqStart, numSamples };
    sequencer.process (t, g, ts, sched, eventScratch);

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

void DYSequencerProcessor::readIncomingMidi (juce::MidiBuffer& midi, int mode)
{
    if (mode == MidiInOff) return;        // pass incoming MIDI straight through

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (mode == MidiInKeyFollow)
        {
            if (m.isNoteOn())
                midiTranspose.store (juce::jlimit (-24, 24, m.getNoteNumber() - 60), std::memory_order_relaxed);
        }
        else if (m.isNoteOn())
        {
            // A new chord starts when nothing was held; notes added while holding join it.
            if (heldNotes.empty())
                chordSize = 0;
            if (std::find (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()) == heldNotes.end())
                heldNotes.push_back (m.getNoteNumber());

            std::vector<int> sorted = heldNotes;            // small; audio thread cost is negligible
            std::sort (sorted.begin(), sorted.end());
            chordSize = juce::jmin (8, static_cast<int> (sorted.size()));
            for (int i = 0; i < chordSize; ++i) chord[i] = sorted[static_cast<size_t> (i)];
            for (int i = 0; i < 8; ++i) uiChord[static_cast<size_t> (i)].store (i < chordSize ? chord[i] : -1);
            uiChordSize.store (chordSize);
        }
        else if (m.isNoteOff())
        {
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
            // released notes stay in the latched chord until a new chord starts
        }
    }
    midi.clear();
}

void DYSequencerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int midiMode = static_cast<int> (std::lround (params.midiIn->load()));
    readIncomingMidi (midi, midiMode);

    GlobalSettings g = params.readGlobal();
    g.sampleRate = currentSampleRate;
    g.midiTranspose = midiMode == MidiInKeyFollow ? midiTranspose.load (std::memory_order_relaxed) : 0;
    if (midiMode == MidiInChordFollow && chordSize > 0)
    {
        g.chordSize = chordSize;
        for (int i = 0; i < chordSize; ++i) g.chordNotes[i] = chord[i];
    }

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
        playing = true;
        ppq     = internalPpq;
    }

    g.bpm = bpm > 1.0 ? bpm : 120.0;
    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
    const auto   ts           = allTrackSettings();
    const double ppqEnd       = ppq + numSamples * ppqPerSample;

    // ---- which pattern plays when
    const int target  = targetPattern();
    int       current = currentPattern.load (std::memory_order_relaxed);
    PatternSchedule sched;
    const bool chain = chainEnabled();

    if (chain)
    {
        const int total = expandChain();
        sched.chain = barMap.data();
        sched.chainBars = total;
        sched.base = &patterns[static_cast<size_t> (target)];
    }
    else
    {
        if (target != current && ! playing)
        {
            currentPattern.store (target, std::memory_order_relaxed);
            current = target;
        }
        sched.base = &patterns[static_cast<size_t> (current)];
        if (target != current && playing)
        {
            sched.next  = &patterns[static_cast<size_t> (target)];
            sched.atPpq = nextBarAtOrAfter (ppq);
        }
    }

    if (playing && looping && loopEnd > loopStart && ppq < loopEnd && ppqEnd > loopEnd + 1e-9)
    {
        // Hosts that do not split buffers at the loop point: play up to the loop end,
        // then continue from the loop start inside the same buffer.
        const int firstLen = juce::jlimit (0, numSamples, static_cast<int> (std::floor ((loopEnd - ppq) / ppqPerSample)));
        runSequencer (ppq, firstLen, 0, true, g, ts, sched, midi);

        PatternSchedule sched2 = sched;
        if (! chain && sched.next != nullptr)
        {
            if (sched.atPpq <= loopEnd + 1e-9) { sched2.base = sched.next; sched2.next = nullptr; }   // switched at the loop end
            else                                 sched2.atPpq = nextBarAtOrAfter (loopStart);
        }
        runSequencer (loopStart, numSamples - firstLen, firstLen, true, g, ts, sched2, midi);
        const double wrappedEnd = loopStart + (numSamples - firstLen) * ppqPerSample;
        currentPattern.store (static_cast<int> (&sched2.at (wrappedEnd - 1e-9) - patterns.data()), std::memory_order_relaxed);
    }
    else
    {
        runSequencer (ppq, numSamples, 0, playing, g, ts, sched, midi);
        if (playing)
            currentPattern.store (static_cast<int> (&sched.at (ppqEnd - 1e-9) - patterns.data()), std::memory_order_relaxed);
    }

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
    const int mode = static_cast<int> (std::lround (params.midiIn->load()));
    if (mode == MidiInChordFollow && uiChordSize.load() > 0)
    {
        int c[8];
        const int n = uiChordSize.load();
        for (int k = 0; k < n; ++k) c[k] = uiChord[static_cast<size_t> (k)].load();
        return noteForChordDegree (c, n, s.transpose);
    }
    const auto g = params.readGlobal();
    const int tr = mode == MidiInKeyFollow ? midiTranspose.load() : 0;
    return juce::jlimit (0, 127, noteForDegree (g.key, g.scale, kBaseNote, s.transpose) + tr);
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
    root.setProperty ("version", 4, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);

    juce::ValueTree names ("Names");
    for (int i = 0; i < kNumTracks; ++i)
        if (trackNames[static_cast<size_t> (i)].isNotEmpty())
            names.setProperty ("t" + juce::String (i), trackNames[static_cast<size_t> (i)], nullptr);
    root.addChild (names, -1, nullptr);

    juce::ValueTree chainTree ("Chain");
    for (int i = 0; i < chainLength.load(); ++i)
    {
        juce::ValueTree e ("Entry");
        e.setProperty ("pattern", chainEntry (i).pattern, nullptr);
        e.setProperty ("bars", chainEntry (i).bars, nullptr);
        chainTree.addChild (e, -1, nullptr);
    }
    root.addChild (chainTree, -1, nullptr);

    for (int pi = 0; pi < kNumPatterns; ++pi)
    {
        juce::ValueTree pat ("Pattern");
        pat.setProperty ("index", pi, nullptr);
        for (int i = 0; i < kNumTracks; ++i)
        {
            const auto& tm = patterns[static_cast<size_t> (pi)].tracks[i];

            bool any = tm.steps() != 16 || tm.pulses() != 0 || tm.rotate() != 0 || tm.euclidMode() != EuclidAdd;
            for (int s = 0; s < kMaxSteps && ! any; ++s) any = tm.isActive (s);
            for (int l = 0; l < static_cast<int> (Lane::Count) && ! any; ++l)
                for (int s = 0; s < kMaxSteps && ! any; ++s)
                    any = tm.get (static_cast<Lane> (l), s) != laneInfo (static_cast<Lane> (l)).def;
            if (! any) continue;

            juce::ValueTree tr ("Track");
            tr.setProperty ("index", i, nullptr);
            tr.setProperty ("steps", tm.steps(), nullptr);
            tr.setProperty ("pulses", tm.pulses(), nullptr);
            tr.setProperty ("rotate", tm.rotate(), nullptr);
            tr.setProperty ("euclid", tm.euclidMode(), nullptr);

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
    const int version = root.getProperty ("version", 1);

    syncingParams = true;                 // the parameter listener must not write into patterns while loading
    auto paramsTree = root.getChildWithName (apvts.state.getType());
    if (paramsTree.isValid())
        apvts.replaceState (paramsTree);
    syncingParams = false;

    for (auto& p : patterns)
        for (auto& t : p.tracks)
        {
            t.clear();
            t.resetSettings();
        }
    for (auto& n : trackNames) n.clear();

    // Older sets had one global set of length / Euclid values: seed every pattern with them.
    if (version < 4)
        copyParamsToAllPatternSettings();

    auto names = root.getChildWithName ("Names");
    if (names.isValid())
        for (int i = 0; i < kNumTracks; ++i)
            trackNames[static_cast<size_t> (i)] = names.getProperty ("t" + juce::String (i), "").toString();

    auto chainTree = root.getChildWithName ("Chain");
    if (chainTree.isValid() && chainTree.getNumChildren() > 0)
    {
        int n = 0;
        for (const auto& e : chainTree)
        {
            if (n >= kMaxChainEntries) break;
            setChainEntry (n++, { static_cast<int> (e.getProperty ("pattern", 0)), static_cast<int> (e.getProperty ("bars", 4)) });
        }
        chainLength.store (n);
    }
    else
    {
        setChainEntry (0, { 0, 4 });
        chainLength.store (1);
    }

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
            if (tr.hasProperty ("steps"))
            {
                tm.setSteps (tr.getProperty ("steps", 16));
                tm.setPulses (tr.getProperty ("pulses", 0));
                tm.setRotate (tr.getProperty ("rotate", 0));
                tm.setEuclidMode (tr.getProperty ("euclid", EuclidAdd));
            }

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

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
    {
        uiTheme         = ui.getProperty ("theme", 0);
        uiScale         = static_cast<float> (static_cast<double> (ui.getProperty ("scale", 1.0)));
        uiExportBars    = ui.getProperty ("exportBars", 4);
        uiSelectedTrack = juce::jlimit (0, kNumTracks - 1, static_cast<int> (ui.getProperty ("selectedTrack", 0)));
    }

    currentPattern.store (targetPattern(), std::memory_order_relaxed);
    settingsDirty.store (true);
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
        // Routing stays with the destination; pattern settings came with the snapshot.
        if (! isRoutingParam (suffix) && ! isPatternSettingParam (suffix) && k < clip.params.size())
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
    settingsDirty.store (true);
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

    std::vector<MidiEvent> events;
    if (chainEnabled())
    {
        // Song mode: export the whole chain once.
        std::array<const PatternModel*, kMaxChainBars> map {};
        int total = 0;
        for (int i = 0; i < chainLength.load() && total < kMaxChainBars; ++i)
        {
            const auto e = chainEntry (i);
            for (int b = 0; b < e.bars && total < kMaxChainBars; ++b)
                map[static_cast<size_t> (total++)] = &patterns[static_cast<size_t> (e.pattern)];
        }
        PatternSchedule sched;
        sched.chain = map.data();
        sched.chainBars = total;
        sched.base = &editPattern();
        bars = total;
        events = Sequencer::renderOffline (g, ts, sched, bars);
    }
    else
    {
        events = Sequencer::renderOffline (g, ts, editPattern(), bars);
    }

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
