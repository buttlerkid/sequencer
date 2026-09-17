#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace dy {

DYSequencerProcessor::DYSequencerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    params.bind (apvts);
    eventScratch.reserve (1024);

    // A friendly starting point: four-on-the-floor on track 1.
    for (int i = 0; i < 16; i += 4)
        pattern.tracks[0].setActive (i, true);
}

TrackSettingsArray DYSequencerProcessor::allTrackSettings() const
{
    TrackSettingsArray ts;
    for (int i = 0; i < kNumTracks; ++i)
        ts[static_cast<size_t> (i)] = trackSettings (i);
    return ts;
}

void DYSequencerProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    sequencer.reset();
    internalPpq = 0.0;
}

bool DYSequencerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void DYSequencerProcessor::runSequencer (double ppqStart, int numSamples, int sampleOffset, bool playing,
                                         const GlobalSettings& g, const TrackSettingsArray& ts, juce::MidiBuffer& midi)
{
    Transport t { playing, ppqStart, numSamples };
    sequencer.process (t, g, ts, pattern, eventScratch);

    const double ppqPerSample = g.bpm / 60.0 / g.sampleRate;
    for (const auto& e : eventScratch)
    {
        int off = playing ? static_cast<int> (std::llround ((e.ppq - ppqStart) / ppqPerSample)) : 0;
        off = juce::jlimit (0, juce::jmax (0, numSamples - 1), off) + sampleOffset;

        midi.addEvent (e.noteOn ? juce::MidiMessage::noteOn (e.channel, e.note, static_cast<juce::uint8> (e.velocity))
                                : juce::MidiMessage::noteOff (e.channel, e.note),
                       off);
    }
}

void DYSequencerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();                       // we make MIDI, not audio

    const int numSamples = buffer.getNumSamples();

    GlobalSettings g = params.readGlobal();
    g.sampleRate = currentSampleRate;

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

    const double ppqEnd = ppq + numSamples * ppqPerSample;
    if (playing && looping && loopEnd > loopStart && ppq < loopEnd && ppqEnd > loopEnd + 1e-9)
    {
        // Hosts that do not split buffers at the loop point: play up to the loop end,
        // then continue from the loop start inside the same buffer.
        const int firstLen = juce::jlimit (0, numSamples, static_cast<int> (std::floor ((loopEnd - ppq) / ppqPerSample)));
        runSequencer (ppq, firstLen, 0, true, g, ts, midi);
        runSequencer (loopStart, numSamples - firstLen, firstLen, true, g, ts, midi);
    }
    else
    {
        runSequencer (ppq, numSamples, 0, playing, g, ts, midi);
    }

    if (! haveHostPpq)
        internalPpq += numSamples * ppqPerSample;

    uiBpm.store (g.bpm);
    uiPpq.store (ppq);
    uiPlaying.store (playing);
    uiInternalClock.store (! haveHostPpq);
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
    root.setProperty ("version", 1, nullptr);
    root.addChild (apvts.copyState(), -1, nullptr);

    juce::ValueTree pat ("Pattern");
    for (int i = 0; i < kNumTracks; ++i)
    {
        const auto& tm = pattern.tracks[i];
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

    auto pat = root.getChildWithName ("Pattern");
    if (pat.isValid())
    {
        for (const auto& tr : pat)
        {
            const int i = tr.getProperty ("index", -1);
            if (i < 0 || i >= kNumTracks) continue;
            auto& tm = pattern.tracks[i];

            const auto act = stringToInts (tr.getProperty ("active").toString());
            for (size_t s = 0; s < act.size() && s < kMaxSteps; ++s) tm.setActive (static_cast<int> (s), act[s] != 0);

            for (int l = 0; l < static_cast<int> (Lane::Count); ++l)
            {
                const auto lane = static_cast<Lane> (l);
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
}

// ------------------------------------------------------------------ clipboard
TrackClip DYSequencerProcessor::makeTrackClip (int i) const
{
    TrackClip c;
    c.steps = pattern.tracks[i].snapshot();
    for (auto* suffix : trackParamSuffixes())
        if (auto* p = apvts.getParameter (ParamIDs::track (i, suffix)))
            c.params.push_back (p->getValue());
    return c;
}

void DYSequencerProcessor::applyTrackClip (int i, const TrackClip& clip)
{
    pattern.tracks[i].load (clip.steps);

    size_t k = 0;
    for (auto* suffix : trackParamSuffixes())
    {
        // Routing stays with the destination track; only musical content is pasted.
        const bool routing = std::strcmp (suffix, ParamIDs::enabled) == 0
                          || std::strcmp (suffix, ParamIDs::mute) == 0
                          || std::strcmp (suffix, ParamIDs::channel) == 0;
        if (! routing && k < clip.params.size())
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
void DYSequencerProcessor::clearTrack (int i) { pattern.tracks[i].clear(); }

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

    const auto events = Sequencer::renderOffline (g, ts, pattern, bars);

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
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Track " + juce::String (i + 1)), 0.0);
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
