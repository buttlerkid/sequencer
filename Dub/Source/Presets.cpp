#include "PluginProcessor.h"

namespace dy::dub {

namespace {
    struct Setter
    {
        juce::AudioProcessorValueTreeState& s;
        void operator() (const juce::String& id, float plain) const
        {
            if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
        }
    };

    void initBanks (Banks& b) { for (auto& bank : b) for (auto& st : bank) st = Step {}; }

    // pluck: fast rise, falls through the step
    const Env kPluck { 0.01f, 0.35f, 0.55f, 0.2f };
    const Env kShort { 0.005f, 0.6f, 0.4f, 0.4f };
    const Env kGate  { 0.01f, 0.06f, 1.0f, 0.0f };

    using Fn = void (*) (const Setter&, Banks&);
    struct Def { const char* name; Fn fn; };

    void init (const Setter&, Banks&) {}

    void dubChords (const Setter& set, Banks& b)
    {
        // the skank: only the off-beats speak, the second one is thrown into the echo
        for (auto& st : b[0]) { st.on = false; st.venv = kPluck; st.cut = 0.72f; st.res = 0.25f; st.fdepth = 1.5f; st.fenv = { 0.0f, 0.0f, 0.0f, 0.4f }; }
        b[0][4].on = true;
        b[0][12].on = true; b[0][12].dly = true; b[0][12].rev = true; b[0][12].pan = 0.2f;
        set (PID::dlyTime, 5); set (PID::dlyFb, 0.6f); set (PID::dlyHp, 400.0f); set (PID::dlyLp, 3500.0f); set (PID::dlyLevel, 0.6f);
        set (PID::revType, RevSpring); set (PID::revLevel, 0.45f); set (PID::revDecay, 0.55f);
    }

    void choppedDrums (const Setter& set, Banks& b)
    {
        static const bool on[16] = { 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1 };
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.on = on[s]; st.venv = { 0.0f, 0.2f, 0.85f, 0.3f }; st.prob = 0.85f;
            st.pan = (s % 2 == 0 ? -0.25f : 0.25f);
            st.vol = (s % 4 == 0) ? 1.0f : 0.75f;
        }
        b[0][7].cut = 0.6f; b[0][7].dly = true;
        b[0][15].cut = 0.55f; b[0][15].res = 0.4f; b[0][15].rev = true;
        set (PID::rate, 4); set (PID::dlyTime, 1); set (PID::dlyFb, 0.35f); set (PID::revType, RevRoom); set (PID::revLevel, 0.4f);
    }

    void fourBarSweep (const Setter& set, Banks& b)
    {
        // the whole bank is one block at 1/4: one filter rise over four bars
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.chain = s < kSteps - 1;
            st.cut = 0.25f; st.res = 0.55f; st.fdepth = 5.5f;
            st.venv = { 0.02f, 0.03f, 1.0f, 0.0f };
            st.fenv = { 0.92f, 0.06f, 1.0f, 0.35f };
        }
        set (PID::rate, 6); set (PID::revType, RevHall); set (PID::revLevel, 0.2f);
    }

    void autoPan (const Setter& set, Banks& b)
    {
        for (int s = 0; s < kSteps; ++s) { b[0][static_cast<size_t> (s)].venv = kGate; b[0][static_cast<size_t> (s)].rev = s % 4 == 3; }
        set (PID::lfo (1, "Target"), ModPan); set (PID::lfo (1, "Amount"), 0.5f); set (PID::lfo (1, "Div"), 4); set (PID::lfo (1, "Shape"), ShapeSine);
        set (PID::lfo (2, "Target"), ModRevSize); set (PID::lfo (2, "Amount"), 0.3f); set (PID::lfo (2, "Div"), 5); set (PID::lfo (2, "Shape"), ShapeTriangle);
        set (PID::revType, RevHall); set (PID::revLevel, 0.3f);
    }

    void tranceGate (const Setter& set, Banks& b)
    {
        static const bool on[16] = { 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 };
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.on = on[s]; st.venv = { 0.01f, 0.25f, 0.9f, 0.0f };
            st.vol = s % 4 == 0 ? 1.0f : 0.7f;
            st.cut = 0.82f; st.fdepth = 1.0f; st.fenv = { 0.0f, 0.0f, 0.0f, 0.3f };
        }
        set (PID::revType, RevPlate); set (PID::revLevel, 0.25f);
        for (int s = 0; s < kSteps; s += 2) b[0][static_cast<size_t> (s)].rev = true;
    }

    void echoThrows (const Setter& set, Banks& b)
    {
        // A plays dry; B throws its last beat into a long, thinning echo
        for (int k = 0; k < kBanks; ++k)
            for (auto& st : b[static_cast<size_t> (k)]) st.venv = kGate;
        for (int s = 12; s < 16; ++s)
        {
            auto& st = b[1][static_cast<size_t> (s)];
            st.dly = true; st.chain = s < 15; st.cut = 0.7f; st.res = 0.35f;
            st.venv = { 0.02f, 0.5f, 0.8f, 0.0f };
        }
        set (PID::pattern, 6);
        set (PID::dlyTime, 5); set (PID::dlyFb, 0.72f); set (PID::dlyHp, 600.0f); set (PID::dlyLp, 4000.0f); set (PID::dlyLevel, 0.7f);
        set (PID::revType, RevChamber); set (PID::revLevel, 0.2f);
    }

    void wobble (const Setter& set, Banks& b)
    {
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.chain = s % 2 == 0;                                // pairs: one wob per quarter
            st.cut = 0.33f; st.res = 0.62f; st.fdepth = 4.0f;
            st.venv = { 0.01f, 0.04f, 1.0f, 0.0f };
            st.fenv = { 0.5f, 0.45f, 1.0f, 0.0f };
            if (s >= 12) st.fdepth = 5.0f;
        }
        set (PID::rate, 4); set (PID::lfo (0, "Target"), ModRes); set (PID::lfo (0, "Amount"), 0.15f); set (PID::lfo (0, "Div"), 5);
        set (PID::revLevel, 0.15f);
    }

    void pump (const Setter& set, Banks& b)
    {
        // a sidechain pump without a sidechain: each quarter ducks and swells back
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.chain = s % 4 != 3;
            st.venv = { 0.55f, 0.02f, 1.0f, 0.55f };
        }
        set (PID::revLevel, 0.0f); set (PID::dlyLevel, 0.0f);
    }

    void springStutter (const Setter& set, Banks& b)
    {
        for (int k = 0; k < kBanks; ++k)
            for (int s = 0; s < kSteps; ++s)
            {
                auto& st = b[static_cast<size_t> (k)][static_cast<size_t> (s)];
                st.venv = kShort;
                st.on = k == 0 ? (s % 3 != 2) : (s < 4 || s % 5 == 0);
                st.rev = k == 1 && s % 5 == 0;
                st.pan = k == 1 ? (s % 2 ? 0.6f : -0.6f) : 0.0f;
                st.prob = k == 1 ? 0.8f : 1.0f;
            }
        set (PID::rate, 0); set (PID::pattern, 2);
        set (PID::revType, RevSpring); set (PID::revLevel, 0.55f); set (PID::revSize, 0.7f);
    }

    void followerDuck (const Setter& set, Banks& b)
    {
        // the louder the input, the darker and drier: hits stay clean, the gaps bloom
        for (int s = 0; s < kSteps; ++s)
        {
            auto& st = b[0][static_cast<size_t> (s)];
            st.venv = kGate; st.rev = true; st.dly = s % 8 == 6;
        }
        set (PID::folTarget, ModRevLevel); set (PID::folAmount, -0.6f); set (PID::folRelease, 400.0f);
        set (PID::revLevel, 0.6f); set (PID::revType, RevPlate);
        set (PID::lfo (0, "Target"), ModCutoff); set (PID::lfo (0, "Amount"), -0.12f); set (PID::lfo (0, "Div"), 6);
    }

    const Def kPresets[] = {
        { "Init", init },
        { "Dub Chords", dubChords },
        { "Chopped Drums", choppedDrums },
        { "Four-Bar Sweep", fourBarSweep },
        { "Auto-Pan Drift", autoPan },
        { "Trance Gate", tranceGate },
        { "Echo Throws (AAAB)", echoThrows },
        { "Wobble Pairs", wobble },
        { "Pump", pump },
        { "Spring Stutter", springStutter },
        { "Follower Bloom", followerDuck },
    };
}

juce::StringArray presetNames()
{
    juce::StringArray n;
    for (auto& d : kPresets) n.add (d.name);
    return n;
}

void applyPreset (int index, juce::AudioProcessorValueTreeState& state, StepStore& steps)
{
    const int n = static_cast<int> (sizeof (kPresets) / sizeof (kPresets[0]));
    index = clampT (index, 0, n - 1);
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    Banks b;
    initBanks (b);
    kPresets[index].fn (Setter { state }, b);
    for (int k = 0; k < kBanks; ++k)
        for (int s = 0; s < kSteps; ++s)
            steps.setStep (k, s, b[static_cast<size_t> (k)][static_cast<size_t> (s)]);
}

} // namespace dy::dub
