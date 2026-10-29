#pragma once
#include <array>
#include <cstdint>
#include <vector>

// DY Mouth engine, the part with no camera and no JUCE: face landmarks -> mouth
// shape -> MIDI. Kept apart so it is unit tested (Tests/MouthTests.cpp).

namespace dy::mouth {

// The four mouth shapes the plugin follows, all 0..1 after calibration.
enum Source { Open = 0, Width, Smile, Pucker, kNumSources };

struct Point { float x = 0, y = 0; };

// The raw geometry of one face, in units of the distance between the outer eye
// corners, measured along the face's own axes (so a tilted head reads the same).
struct MouthShape
{
    float gap = 0;        // inner lips apart
    float width = 0;      // mouth corner to corner
    float lift = 0;       // corners above the top of the upper lip (higher = smile)
};

// MediaPipe face mesh indices used here.
namespace Mesh
{
    constexpr int kCount = 478;
    constexpr int eyeR = 33, eyeL = 263, lipTop = 0, innerTop = 13, innerBottom = 14, cornerR = 61, cornerL = 291;
    // the outer and inner lip contours, for drawing
    constexpr std::array<int, 20> outerLips { 61, 185, 40, 39, 37, 0, 267, 269, 270, 409, 291, 375, 321, 405, 314, 17, 84, 181, 91, 146 };
    constexpr std::array<int, 20> innerLips { 78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308, 324, 318, 402, 317, 14, 87, 178, 88, 95 };
}

MouthShape measure (const Point* landmarks);   // landmarks: Mesh::kCount points

// The resting face and how far each shape goes, set by the Neutral button.
struct Calibration
{
    float gapRest = 0.02f, widthRest = 0.58f, liftRest = -0.10f;
    float gapSpan = 0.45f, widthSpan = 0.16f, liftSpan = 0.07f, puckerSpan = 0.12f;
};

std::array<float, kNumSources> normalise (const MouthShape&, const Calibration&);

// ------------------------------------------------------------------ MIDI out
enum Dest { DestOff = 0, DestCC, DestPitchBend, DestPressure };

struct SlotParams
{
    int source = Open;
    int dest = DestCC;
    int cc = 1;
    float smoothMs = 60.0f;
    float lo = 0.0f, hi = 1.0f;     // output range
    bool invert = false;
};

struct MapperParams
{
    int channel = 1;
    std::array<SlotParams, 4> slots {};
    bool trigger = true;            // mouth open plays a note
    int triggerNote = 48;
    float threshold = 0.25f;        // of Open
    bool velocityFromSpeed = true;  // a quick open plays louder
    int velocity = 100;
};

struct MidiOut
{
    enum Type { NoteOn, NoteOff, CC, PitchBend, Pressure } type;
    int channel, data1, data2;      // PitchBend: data1 = 0..16383
};

class MouthMapper
{
public:
    void setParams (const MapperParams&);
    const MapperParams& getParams() const { return params; }

    // One audio block. `target` is the normalised shape (0..1), `face` false while
    // nobody is in view (outputs hold, a playing note is released).
    void process (const std::array<float, kNumSources>& target, bool face, double blockSeconds, std::vector<MidiOut>& out);
    void allOff (std::vector<MidiOut>& out);

    float slotValue (int i) const { return value[(size_t) i]; }    // smoothed, 0..1, before range
    float slotOutput (int i) const { return output[(size_t) i]; }  // what is sent, 0..1
    bool noteSounding() const { return sounding >= 0; }
    float gate() const { return gateLevel; }                       // 0..1, the open amount while a note sounds

private:
    MapperParams params;
    std::array<float, 4> value {}, output {};
    std::array<int, 4> lastSent { -1, -1, -1, -1 };
    std::array<bool, 4> primed {};
    int sounding = -1, soundingChannel = 1;
    float lastOpen = 0.0f, gateLevel = 0.0f;
    double armTime = 0.0;            // how long the mouth took from half the threshold to it
};

} // namespace dy::mouth
