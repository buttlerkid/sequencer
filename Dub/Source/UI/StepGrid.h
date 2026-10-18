#pragma once
#include "../PluginProcessor.h"
#include "DubLookAndFeel.h"

namespace dy::dub {

// The 16 steps of one bank, lane by lane: draw values by dragging across the bars,
// click toggles (drag to paint the same state), click a step number to edit its envelopes.
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (DubProcessor& p) : proc (p) { visible = allLanes(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverLane = hoverStep = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override;

    int bank = 0;                  // the bank on view
    int selected = 0;              // the step whose envelopes are edited
    std::function<void (int)> onSelect;

    enum Kind { Toggle, Bar, Bipolar, Link };
    struct Lane { const char* name; int field; Kind kind; float lo, hi, def; const char* tip; };
    static const std::vector<Lane>& allLanes();
    const std::vector<Lane>& lanes() const { return visible; }
    // show only these lanes (indices into allLanes), with this label column and row heights
    void configure (std::vector<int> laneIndices, int labelWidth, int barHeight, int toggleHeight);

private:
    std::vector<Lane> visible;
    int kLabelW = 62, kBarH = 34, kToggleH = 20;
    static constexpr int kHeaderH = 24, kGap = 3;
    int laneY (int lane) const;
    int laneH (int lane) const;
    juce::Rectangle<float> cell (int lane, int step) const;
    int stepAt (float x) const;
    int laneAt (float y) const;
    float valueAt (int lane, float y) const;
    juce::String valueText (int lane, float v) const;
    void edit (int lane, int step, float y);
    void showMenu (int lane, int step);

    DubProcessor& proc;
    int dragLane = -1, lastStep = -1, hoverLane = -1, hoverStep = -1;
    float paintState = 0.0f;
};

} // namespace dy::dub
