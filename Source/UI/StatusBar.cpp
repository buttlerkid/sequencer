#include "StatusBar.h"
#include "Theme.h"

namespace dy {

void StatusBar::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds();

    g.setColour (t.textDim);
    g.setFont (uiFont (11.0f));

    const bool playing = proc.uiPlaying.load();
    const double ppq   = proc.uiPpq.load();
    const int bar  = static_cast<int> (std::floor (ppq / 4.0)) + 1;
    const int beat = static_cast<int> (std::floor (std::fmod (juce::jmax (0.0, ppq), 4.0))) + 1;

    juce::String transport = juce::String (proc.uiBpm.load(), 1) + " BPM   ";
    transport += playing ? juce::String::charToString (0x25B6) + "  " + juce::String (bar) + "." + juce::String (beat)
                         : juce::String::charToString (0x25A0) + "  stopped";
    if (proc.uiInternalClock.load())
        transport += "   (internal clock)";

    g.drawText (transport, r.removeFromRight (280).reduced (8, 0), juce::Justification::centredRight);

    g.setColour (t.textDim);
    g.drawText (hint.isNotEmpty() ? hint
                                  : juce::String ("Click steps to toggle, drag to paint. Click a pad to audition. Double-click a name to rename. Drag Export MIDI onto a track."),
                r.reduced (8, 0), juce::Justification::centredLeft);
}

} // namespace dy
