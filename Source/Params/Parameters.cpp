#include "Parameters.h"
#include "Engine/Scale.h"
#include "Engine/Shuffle.h"

namespace dy {

const juce::StringArray kDivisionNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
const double            kDivisionPpq[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
const int               kNumDivisions = 8;

juce::StringArray keyNames()
{
    juce::StringArray a;
    for (auto* n : kKeyNames) a.add (n);
    return a;
}

juce::StringArray scaleNames()
{
    juce::StringArray a;
    for (int i = 0; i < kNumScales; ++i) a.add (kScales[i].name);
    return a;
}

juce::StringArray shuffleProfileNames()
{
    juce::StringArray a;
    for (int i = 0; i < kNumShuffleProfiles; ++i) a.add (kShuffleProfiles[i].name);
    return a;
}

juce::StringArray euclidModeNames() { return { "Off", "Add", "Only" }; }
juce::StringArray pitchModeNames()  { return { "Scale", "Fixed" }; }
juce::StringArray swingModeNames()  { return { "Global", "Off", "Custom" }; }

const std::vector<const char*>& trackParamSuffixes()
{
    static const std::vector<const char*> s {
        ParamIDs::enabled, ParamIDs::mute, ParamIDs::channel, ParamIDs::steps, ParamIDs::division,
        ParamIDs::pulses, ParamIDs::rotate, ParamIDs::euclidMode, ParamIDs::pitchMode, ParamIDs::fixedNote,
        ParamIDs::transpose, ParamIDs::swingMode, ParamIDs::swingProfileT, ParamIDs::swingAmountT
    };
    return s;
}

static juce::ParameterID pid (const juce::String& id) { return { id, 1 }; }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::key),          "Key",           keyNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::scale),        "Scale",         scaleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::swingProfile), "Swing Profile", shuffleProfileNames(), 0));
    layout.add (std::make_unique<AudioParameterInt>    (pid (ParamIDs::swingAmount),  "Swing Amount",  0, 100, 0,
                                                        AudioParameterIntAttributes().withLabel ("%")));

    for (int i = 0; i < kNumTracks; ++i)
    {
        const String prefix = "T" + String (i + 1) + " ";
        auto id = [i] (const char* suffix) { return pid (ParamIDs::track (i, suffix)); };

        auto group = std::make_unique<AudioProcessorParameterGroup> ("track" + String (i + 1), "Track " + String (i + 1), "|");
        group->addChild (std::make_unique<AudioParameterBool>   (id (ParamIDs::enabled),    prefix + "On",        i == 0));
        group->addChild (std::make_unique<AudioParameterBool>   (id (ParamIDs::mute),       prefix + "Mute",      false));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::channel),    prefix + "Channel",   1, 16, i + 1));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::steps),      prefix + "Steps",     1, kMaxSteps, 16));
        group->addChild (std::make_unique<AudioParameterChoice> (id (ParamIDs::division),   prefix + "Division",  kDivisionNames, 3));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::pulses),     prefix + "Pulses",    0, kMaxSteps, 0));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::rotate),     prefix + "Rotate",    0, kMaxSteps - 1, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (id (ParamIDs::euclidMode), prefix + "Euclid",    euclidModeNames(), EuclidAdd));
        group->addChild (std::make_unique<AudioParameterChoice> (id (ParamIDs::pitchMode),  prefix + "Pitch",     pitchModeNames(), PitchScale));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::fixedNote),  prefix + "Note",      0, 127, 36 + i));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::transpose),  prefix + "Transpose", -21, 21, 0,
                                                                 AudioParameterIntAttributes().withLabel ("deg")));
        group->addChild (std::make_unique<AudioParameterChoice> (id (ParamIDs::swingMode),  prefix + "Swing",     swingModeNames(), SwingGlobal));
        group->addChild (std::make_unique<AudioParameterChoice> (id (ParamIDs::swingProfileT), prefix + "Swing Profile", shuffleProfileNames(), 0));
        group->addChild (std::make_unique<AudioParameterInt>    (id (ParamIDs::swingAmountT),  prefix + "Swing Amount",  0, 100, 50,
                                                                 AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::move (group));
    }

    return layout;
}

static int asInt (const std::atomic<float>* p) { return p ? static_cast<int> (std::lround (p->load())) : 0; }

TrackSettings TrackParamRefs::read() const
{
    TrackSettings s;
    s.enabled      = asInt (enabled) != 0;
    s.mute         = asInt (mute) != 0;
    s.channel      = asInt (channel);
    s.steps        = asInt (steps);
    s.division     = kDivisionPpq[clampT (asInt (division), 0, kNumDivisions - 1)];
    s.pulses       = asInt (pulses);
    s.rotate       = asInt (rotate);
    s.euclidMode   = asInt (euclidMode);
    s.pitchMode    = asInt (pitchMode);
    s.fixedNote    = asInt (fixedNote);
    s.transpose    = asInt (transpose);
    s.swingMode    = asInt (swingMode);
    s.swingProfile = asInt (swingProfile);
    s.swingAmount  = asInt (swingAmount);
    return s;
}

void ParamRefs::bind (juce::AudioProcessorValueTreeState& apvts)
{
    key          = apvts.getRawParameterValue (ParamIDs::key);
    scale        = apvts.getRawParameterValue (ParamIDs::scale);
    swingProfile = apvts.getRawParameterValue (ParamIDs::swingProfile);
    swingAmount  = apvts.getRawParameterValue (ParamIDs::swingAmount);

    for (int i = 0; i < kNumTracks; ++i)
    {
        auto get = [&] (const char* suffix) { return apvts.getRawParameterValue (ParamIDs::track (i, suffix)); };
        auto& t = tracks[i];
        t.enabled      = get (ParamIDs::enabled);
        t.mute         = get (ParamIDs::mute);
        t.channel      = get (ParamIDs::channel);
        t.steps        = get (ParamIDs::steps);
        t.division     = get (ParamIDs::division);
        t.pulses       = get (ParamIDs::pulses);
        t.rotate       = get (ParamIDs::rotate);
        t.euclidMode   = get (ParamIDs::euclidMode);
        t.pitchMode    = get (ParamIDs::pitchMode);
        t.fixedNote    = get (ParamIDs::fixedNote);
        t.transpose    = get (ParamIDs::transpose);
        t.swingMode    = get (ParamIDs::swingMode);
        t.swingProfile = get (ParamIDs::swingProfileT);
        t.swingAmount  = get (ParamIDs::swingAmountT);
    }
}

GlobalSettings ParamRefs::readGlobal() const
{
    GlobalSettings g;
    g.key          = asInt (key);
    g.scale        = asInt (scale);
    g.swingProfile = asInt (swingProfile);
    g.swingAmount  = asInt (swingAmount);
    return g;
}

} // namespace dy
