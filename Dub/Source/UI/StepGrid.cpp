#include "StepGrid.h"
#include "NodalLookAndFeel.h"

namespace dy::dub {

namespace Colours = dy::nodal::Colours;
using dy::nodal::displayFont;
using dy::nodal::bodyFont;
using dy::nodal::monoFont;

const std::vector<StepGrid::Lane>& StepGrid::lanes()
{
    using F = StepStore;
    static const std::vector<Lane> l = {
        { "ON",    F::On,     Toggle,  0, 1, 1, "Step on / off: off steps leave a gap" },
        { "VOL",   F::Vol,    Bar,     0, 1, 1, "Level of the step" },
        { "CUT",   F::Cut,    Bar,     0, 1, 1, "Filter cutoff of the step" },
        { "RES",   F::Res,    Bar,     0, 1, 0, "Filter resonance of the step" },
        { "PAN",   F::Pan,    Bipolar, -1, 1, 0, "Pan of the step" },
        { "PROB",  F::Prob,   Bar,     0, 1, 1, "Chance the step plays at all" },
        { "F.ENV", F::FDepth, Bipolar, -5, 5, 0, "Filter envelope depth in octaves (shape: click the step number)" },
        { "DLY",   F::Dly,    Toggle,  0, 1, 0, "Send this step into the delay" },
        { "REV",   F::Rev,    Toggle,  0, 1, 0, "Send this step into the reverb" },
        { "CHAIN", F::Chain,  Link,    0, 1, 0, "Join this step to the next: one longer step, one envelope across the block" },
    };
    return l;
}

int StepGrid::laneH (int lane) const
{
    const auto k = lanes()[static_cast<size_t> (lane)].kind;
    return k == Bar || k == Bipolar ? kBarH : kToggleH;
}

int StepGrid::laneY (int lane) const
{
    int y = kHeaderH + kGap;
    for (int i = 0; i < lane; ++i) y += laneH (i) + kGap;
    return y;
}

juce::Rectangle<float> StepGrid::cell (int lane, int step) const
{
    const float w = (getWidth() - kLabelW) / static_cast<float> (kSteps);
    const float y = lane < 0 ? 0.0f : static_cast<float> (laneY (lane));
    const float h = lane < 0 ? static_cast<float> (kHeaderH) : static_cast<float> (laneH (lane));
    return { kLabelW + step * w + 1.5f, y, w - 3.0f, h };
}

int StepGrid::stepAt (float x) const
{
    const float w = (getWidth() - kLabelW) / static_cast<float> (kSteps);
    return clampT (static_cast<int> ((x - kLabelW) / w), 0, kSteps - 1);
}

int StepGrid::laneAt (float y) const
{
    if (y < kHeaderH) return -1;
    for (int i = 0; i < static_cast<int> (lanes().size()); ++i)
        if (y >= laneY (i) - kGap * 0.5f && y < laneY (i) + laneH (i) + kGap * 0.5f) return i;
    return -2;
}

float StepGrid::valueAt (int lane, float y) const
{
    const auto& L = lanes()[static_cast<size_t> (lane)];
    const float top = laneY (lane) + 2.0f, h = laneH (lane) - 4.0f;
    const float t = clampT (1.0f - (y - top) / h, 0.0f, 1.0f);
    float v = L.lo + t * (L.hi - L.lo);
    if (L.kind == Bipolar && std::abs (v) < (L.hi - L.lo) * 0.03f) v = 0.0f;       // snap to the middle
    return v;
}

juce::String StepGrid::valueText (int lane, float v) const
{
    switch (lanes()[static_cast<size_t> (lane)].field)
    {
        case StepStore::Cut:
        {
            const double hz = cutToHz (v * 10.0);
            return hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + "k" : juce::String (juce::roundToInt (hz));
        }
        case StepStore::Pan:
        {
            const int p = juce::roundToInt (v * 100.0f);
            return p == 0 ? juce::String ("C") : juce::String (std::abs (p)) + (p < 0 ? "L" : "R");
        }
        case StepStore::FDepth: return (v > 0.005f ? "+" : "") + juce::String (v, 1);
        default: return juce::String (juce::roundToInt (v * 100.0f));
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    auto& st = proc.steps;
    const int len = static_cast<int> (proc.apvts.getRawParameterValue (PID::length)->load());
    const auto& tel = proc.telemetry();
    const bool running = tel.running.load() && tel.step.load() >= 0;
    const int playBank = tel.bank.load(), playStep = tel.step.load();
    const int playBlock = tel.blockStart.load(), playLen = tel.blockLen.load();
    const bool showPlay = running && playBank == bank;

    // blocks: each step's block start
    int blockOf[kSteps];
    for (int s = 0; s < kSteps; ++s)
        blockOf[s] = (s > 0 && s < len && st.get (bank, s - 1, StepStore::Chain) > 0.5f) ? blockOf[s - 1] : s;

    // beat shading behind the columns
    for (int s = 0; s < kSteps; ++s)
    {
        auto col = cell (-1, s).withBottom (static_cast<float> (getHeight()));
        if ((s / 4) % 2 == 1) { g.setColour (Colours::panel2.withAlpha (0.35f)); g.fillRect (col.expanded (1.5f, 0.0f)); }
    }
    if (showPlay && playStep >= 0)
    {
        auto col = cell (-1, playStep).withBottom (static_cast<float> (getHeight())).expanded (1.5f, 0.0f);
        g.setColour (Colours::brass.withAlpha (tel.played.load() ? 0.10f : 0.04f));
        g.fillRect (col);
    }

    // header: step numbers, chained blocks, selection, block progress
    for (int s = 0; s < kSteps; ++s)
    {
        auto r = cell (-1, s).reduced (0.0f, 3.0f);
        const bool sel = blockOf[s] == blockOf[clampT (selected, 0, kSteps - 1)];
        const bool inLen = s < len;
        g.setColour (sel ? Colours::brass.withAlpha (0.18f) : Colours::panel2);
        g.fillRoundedRectangle (r, 4.0f);
        if (sel) { g.setColour (Colours::brass.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); }
        g.setColour (inLen ? (showPlay && s == playStep ? Colours::brass : Colours::text) : Colours::dim.withAlpha (0.4f));
        g.setFont (monoFont (11.0f));
        g.drawText (juce::String (s + 1), r, juce::Justification::centred);
    }
    if (showPlay)
    {
        auto a = cell (-1, playBlock), b = cell (-1, juce::jmin (kSteps - 1, playBlock + playLen - 1));
        const float x0 = a.getX(), x1 = b.getRight();
        g.setColour (Colours::brass);
        g.fillRect (x0, static_cast<float> (kHeaderH) - 2.0f, (x1 - x0) * tel.blockPhase.load(), 2.0f);
    }

    g.setFont (displayFont (9.5f));
    g.setColour (Colours::dim);
    g.drawText ("STEP", 4, 0, kLabelW - 8, kHeaderH, juce::Justification::centredLeft);

    const auto& L = lanes();
    for (int li = 0; li < static_cast<int> (L.size()); ++li)
    {
        const auto& lane = L[static_cast<size_t> (li)];
        const int y = laneY (li), h = laneH (li);
        g.setColour (li == hoverLane || li == dragLane ? Colours::text : Colours::dim);
        g.setFont (displayFont (9.5f));
        g.drawText (lane.name, 4, y, kLabelW - 8, h, juce::Justification::centredLeft);

        for (int s = 0; s < kSteps; ++s)
        {
            auto r = cell (li, s);
            const bool inLen = s < len;
            const bool follower = blockOf[s] != s;            // inside a chained block: the first step's values play
            const float alpha = ! inLen ? 0.3f : (follower && lane.kind != Link ? 0.45f : 1.0f);
            const float v = st.get (bank, s, lane.field);

            if (lane.kind == Toggle)
            {
                const bool on = v > 0.5f;
                juce::Colour c = lane.field == StepStore::Dly ? Colours::teal : lane.field == StepStore::Rev ? Colours::lilac : Colours::brass;
                g.setColour ((on ? c : Colours::panel2).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (r, 3.0f);
                if (! on) { g.setColour (Colours::line.withMultipliedAlpha (alpha)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
            }
            else if (lane.kind == Link)
            {
                const bool can = s < len - 1;
                const bool on = v > 0.5f && can;
                g.setColour (Colours::panel2.withMultipliedAlpha (can ? 1.0f : 0.3f));
                g.fillRoundedRectangle (r, 3.0f);
                if (on)
                {
                    // a bar reaching into the next step
                    auto bar = r.withSizeKeepingCentre (r.getWidth(), 6.0f).withRight (r.getRight() + 3.0f);
                    if (s > 0 && blockOf[s] != s) bar.setLeft (r.getX() - 3.0f);
                    g.setColour (Colours::brass);
                    g.fillRect (bar);
                }
                else if (s > 0 && blockOf[s] != s)
                {
                    auto bar = r.withSizeKeepingCentre (r.getWidth(), 6.0f).withWidth (r.getWidth() * 0.5f).withX (r.getX() - 3.0f);
                    g.setColour (Colours::brass);
                    g.fillRect (bar);
                }
            }
            else
            {
                g.setColour (Colours::scopeBg.withMultipliedAlpha (inLen ? 1.0f : 0.5f));
                g.fillRoundedRectangle (r, 3.0f);
                auto inner = r.reduced (2.0f);
                const float t = (v - lane.lo) / (lane.hi - lane.lo);
                juce::Colour c = lane.field == StepStore::FDepth ? Colours::teal : Colours::brass;
                g.setColour (c.withMultipliedAlpha (alpha * 0.85f));
                if (lane.kind == Bar)
                    g.fillRoundedRectangle (inner.withTop (inner.getBottom() - inner.getHeight() * t), 2.0f);
                else
                {
                    const float mid = inner.getCentreY(), yv = inner.getBottom() - inner.getHeight() * t;
                    g.fillRect (inner.getX(), std::min (mid, yv), inner.getWidth(), std::max (1.0f, std::abs (yv - mid)));
                    g.setColour (Colours::line);
                    g.drawHorizontalLine (juce::roundToInt (mid), inner.getX(), inner.getRight());
                }
                if ((li == dragLane && s == lastStep) || (dragLane < 0 && li == hoverLane && s == hoverStep))
                {
                    g.setColour (Colours::text);
                    g.setFont (monoFont (10.0f));
                    g.drawText (valueText (li, v), r.withHeight (12.0f).translated (0.0f, 1.0f), juce::Justification::centred);
                }
            }
        }
    }
}

void StepGrid::edit (int lane, int step, float y)
{
    const auto& L = lanes()[static_cast<size_t> (lane)];
    if (L.kind == Toggle || L.kind == Link)
    {
        if (L.kind == Link && step >= kSteps - 1) return;
        proc.steps.set (bank, step, L.field, paintState);
    }
    else
        proc.steps.set (bank, step, L.field, valueAt (lane, y));
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.y), step = stepAt (e.position.x);
    if (e.position.x < kLabelW) return;
    if (lane == -1)
    {
        selected = step;
        if (onSelect) onSelect (step);
        repaint();
        return;
    }
    if (lane < 0) return;
    if (e.mods.isPopupMenu()) { showMenu (lane, step); return; }
    const auto& L = lanes()[static_cast<size_t> (lane)];
    dragLane = lane;
    lastStep = step;
    if (L.kind == Toggle || L.kind == Link) paintState = proc.steps.get (bank, step, L.field) > 0.5f ? 0.0f : 1.0f;
    edit (lane, step, e.position.y);
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0) return;
    const int step = stepAt (e.position.x);
    const int a = std::min (step, lastStep), b = std::max (step, lastStep);
    for (int s = a; s <= b; ++s) edit (dragLane, s, e.position.y);       // fast drags fill every step they cross
    lastStep = step;
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&) { dragLane = -1; repaint(); }

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.y), step = stepAt (e.position.x);
    if (lane != hoverLane || step != hoverStep) { hoverLane = lane; hoverStep = step; repaint(); }
    if (lane >= 0) setTooltip (juce::String (lanes()[static_cast<size_t> (lane)].tip) + "   (right-click for lane tools)");
    else if (lane == -1) setTooltip ("Click a step number to edit its volume and filter envelopes");
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int lane = laneAt (e.position.y);
    if (lane < 0 || e.position.x < kLabelW) return;
    const auto& L = lanes()[static_cast<size_t> (lane)];
    if (L.kind == Bar || L.kind == Bipolar) { proc.steps.set (bank, stepAt (e.position.x), L.field, L.def); repaint(); }
}

void StepGrid::showMenu (int lane, int step)
{
    const auto L = lanes()[static_cast<size_t> (lane)];
    const float v = proc.steps.get (bank, step, L.field);
    juce::PopupMenu m;
    m.addSectionHeader (juce::String (L.name) + " lane, bank " + (bank == 0 ? "A" : "B"));
    m.addItem (1, "Set every step to this one's value");
    m.addItem (2, "Reset lane");
    m.addItem (3, "Randomise lane");
    m.addItem (4, "Copy lane to bank " + juce::String (bank == 0 ? "B" : "A"));
    if (L.kind == Toggle) m.addItem (5, "Invert lane");
    juce::Component::SafePointer<StepGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, L, v] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& st = safe->proc.steps;
        const int b = safe->bank;
        juce::Random rnd;
        for (int s = 0; s < kSteps; ++s)
        {
            switch (r)
            {
                case 1: st.set (b, s, L.field, v); break;
                case 2: st.set (b, s, L.field, L.def); break;
                case 3:
                    if (L.kind == Toggle || L.kind == Link) st.set (b, s, L.field, rnd.nextFloat() < 0.35f ? 1.0f : 0.0f);
                    else st.set (b, s, L.field, L.lo + rnd.nextFloat() * (L.hi - L.lo));
                    break;
                case 4: st.set (1 - b, s, L.field, st.get (b, s, L.field)); break;
                case 5: st.set (b, s, L.field, st.get (b, s, L.field) > 0.5f ? 0.0f : 1.0f); break;
                default: break;
            }
        }
        safe->repaint();
    });
}

} // namespace dy::dub
