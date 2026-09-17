#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Engine/Types.h"

namespace dy {

// Editor is designed at this logical size and scaled uniformly to the window.
constexpr int kLogicalW = 1200;
constexpr int kLogicalH = 600;
constexpr int kTrackListW = 200;
constexpr int kHeaderH = 52;
constexpr int kStatusH = 30;

// Column geometry shared by the step grid and lane editors so they line up.
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

} // namespace dy
