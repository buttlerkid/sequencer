#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Engine/Types.h"

namespace dy {

// Editor is designed at this logical size and scaled uniformly to the window.
constexpr int kLogicalW = 1200;
constexpr int kLogicalH = 740;
constexpr int kHeaderH  = 48;
constexpr int kChainH   = 30;
constexpr int kStatusH  = 26;

// Overview grid
constexpr int kRowH            = 24;
constexpr int kOverviewHeaderH = 20;
constexpr int kAddRowH         = 26;
constexpr int kMaxVisibleRows  = 10;

// Fixed columns at the left of every overview row (logical px).
namespace cols
{
    constexpr int index  = 24;
    constexpr int name   = 96;
    constexpr int note   = 46;
    constexpr int shift  = 48;
    constexpr int mute   = 22;
    constexpr int solo   = 22;
    constexpr int gap    = 6;
    constexpr int steps  = 34;
    constexpr int pulses = 34;
    constexpr int rotate = 34;
    constexpr int remove = 18;
    constexpr int controls = index + name + note + shift + mute + solo + gap + steps + pulses + rotate + gap;
}

// Column geometry shared by the overview cells and the lane editor so they line up.
inline juce::Rectangle<float> stepColumn (juce::Rectangle<int> area, int steps, int i, float gap = 2.0f)
{
    const float w = area.getWidth() / static_cast<float> (juce::jmax (1, steps));
    return { area.getX() + i * w + gap * 0.5f, static_cast<float> (area.getY()), w - gap, static_cast<float> (area.getHeight()) };
}

inline int stepAtX (juce::Rectangle<int> area, int steps, float x)
{
    const float w = area.getWidth() / static_cast<float> (juce::jmax (1, steps));
    return clampT (static_cast<int> (std::floor ((x - area.getX()) / w)), 0, juce::jmax (1, steps) - 1);
}

// Whether a step fires given the manual state, the Euclid hit and the mode.
inline bool stepFires (int euclidMode, bool manual, bool euclidHit)
{
    return euclidMode == EuclidOff ? manual : euclidMode == EuclidAdd ? (manual || euclidHit) : euclidHit;
}

} // namespace dy
