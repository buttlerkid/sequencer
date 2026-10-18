#pragma once
#include "../PluginProcessor.h"

namespace dy::dub {

// The volume and filter envelopes of one step (or chained block), drawn over its
// length. Drag the rise node sideways, the sustain node anywhere, and the curve
// itself up or down to bend it.
class EnvEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EnvEditor (DubProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = DragNone; }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    int bank = 0, step = 0;

private:
    enum DragKind { DragNone, DragRise, DragSus, DragCurve };
    int blockStart() const;
    int blockLen() const;
    juce::Rectangle<float> graph (int which) const;          // 0 volume, 1 filter
    juce::Point<float> node (int which, int n) const;        // n 0 = rise, 1 = sustain / fall
    int fieldBase (int which) const { return which == 0 ? StepStore::VRise : StepStore::FRise; }
    void copyTo (bool bothBanks);

    DubProcessor& proc;
    juce::TextButton allSteps { "Copy to bank" }, allBanks { "Copy to both banks" };
    DragKind drag = DragNone;
    int dragWhich = 0;
    float startCurve = 0.0f, startY = 0.0f;
};

} // namespace dy::dub
