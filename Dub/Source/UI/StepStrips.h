#pragma once
#include "../PluginProcessor.h"
#include "DubLookAndFeel.h"

namespace dy::dub {

class StripArea;

// One step as a console channel: CUT and RES knobs, a VOL fader with a level meter
// beside it, and an ON key. Click the strip's number to edit its envelopes.
class StepStrip : public juce::Component
{
public:
    StepStrip (StripArea& owner, DubProcessor& p, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void sync();                                         // values from the step store
    void setState (bool selected, bool playing, bool follower, bool inLength, float meter);

    const int index;

private:
    struct OnKey : public juce::Component
    {
        explicit OnKey (StepStrip& s) : strip (s) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        StepStrip& strip;
    };

    juce::String caption (juce::Slider&, const char* name) const;
    void setupKnob (juce::Slider&, int field, double def, std::function<juce::String (double)> text);

    StripArea& area;
    DubProcessor& proc;
    juce::Slider cut, res, vol;
    OnKey onKey { *this };
    bool selected = false, playing = false, follower = false, inLength = true, syncing = false;
    float meter = 0.0f;
    juce::Rectangle<int> rNum, rCutCap, rResCap, rVolCap, rMeter;

    friend class StripArea;
};

// The chain link between two strips.
class LinkButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    LinkButton (StripArea& a, int left) : area (a), leftStep (left) { setTooltip ("Chain: join this step to the next one (one longer step, one envelope across both)"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool on = false, enabled = true;
private:
    StripArea& area;
    int leftStep;
};

class StripArea : public juce::Component
{
public:
    explicit StripArea (DubProcessor& p);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void tick();                                         // 30 Hz from the editor

    void setBank (int b);
    int getBank() const { return bank; }
    void select (int step);
    int selected = 0;
    std::function<void (int)> onSelect;
    static constexpr int kGutter = 60;

private:
    friend class StepStrip;
    friend class LinkButton;
    DubProcessor& proc;
    std::vector<std::unique_ptr<StepStrip>> strips;
    std::vector<std::unique_ptr<LinkButton>> links;
    int bank = 0, shownVersion = -1, playBank = -1;
    float meterHold = 0.0f;
};

} // namespace dy::dub
