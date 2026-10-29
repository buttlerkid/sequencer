#pragma once
#include <array>
#include <cstdint>
#include <vector>

// DY Mouth engine, the part with no camera and no JUCE: face landmarks -> face
// shapes -> MIDI. Kept apart so it is unit tested (Tests/MouthTests.cpp).

namespace dy::mouth {

constexpr int kMaxFaces = 4;

// What the plugin follows on each face, all 0..1 after calibration.
enum Source { Open = 0, Width, Smile, Pucker, Brows, Tilt, HeadX, HeadY, kNumSources };
using Shapes = std::array<float, kNumSources>;

// Every face in view this frame (by face number; a face keeps its number while it stays).
struct Faces
{
    std::array<Shapes, kMaxFaces> shapes {};
    std::array<bool, kMaxFaces> present {};
};

struct Point { float x = 0, y = 0; };

// The raw geometry of one face, in units of the distance between the outer eye
// corners, measured along the face's own axes (so a tilted head reads the same).
struct MouthShape
{
    float gap = 0;        // inner lips apart
    float width = 0;      // mouth corner to corner
    float lift = 0;       // corners above the top of the upper lip (higher = smile)
    float brow = 0;       // eyebrows above the eyelids
    float roll = 0;       // head tilt, radians, + = towards the person's right shoulder
    float headX = 0.5f, headY = 0.5f;   // nose in the frame, 0..1, as the person sees it (mirrored, up = 1)
};

// MediaPipe face mesh indices used here.
namespace Mesh
{
    constexpr int kCount = 478;
    constexpr int eyeR = 33, eyeL = 263, lipTop = 0, innerTop = 13, innerBottom = 14, cornerR = 61, cornerL = 291;
    constexpr int browR = 105, browL = 334, lidR = 159, lidL = 386, nose = 1;
    // the outer and inner lip contours, for drawing
    constexpr std::array<int, 20> outerLips { 61, 185, 40, 39, 37, 0, 267, 269, 270, 409, 291, 375, 321, 405, 314, 17, 84, 181, 91, 146 };
    constexpr std::array<int, 20> innerLips { 78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308, 324, 318, 402, 317, 14, 87, 178, 88, 95 };
}

// landmarks: Mesh::kCount points; the frame size places the head (headX / headY)
MouthShape measure (const Point* landmarks, float frameWidth = 0, float frameHeight = 0);

// The resting face and how far each shape goes; the rests are set by Neutral.
struct Calibration
{
    float gapRest = 0.02f, widthRest = 0.58f, liftRest = -0.10f, browRest = 0.18f, rollRest = 0.0f;
    float gapSpan = 0.45f, widthSpan = 0.16f, liftSpan = 0.07f, puckerSpan = 0.12f, browSpan = 0.08f, rollSpan = 0.45f;
};

Shapes normalise (const MouthShape&, const Calibration&);

// ------------------------------------------------------------------ MIDI out
enum Dest { DestOff = 0, DestCC, DestPitchBend, DestPressure };
enum PitchMode { PitchChord = 0, PitchHeight };

struct SlotParams
{
    int source = Open;
    int dest = DestCC;
    int cc = 1;
    float smoothMs = 60.0f;
    float lo = 0.0f, hi = 1.0f;     // output range
    bool invert = false;
    int face = 0;
};

struct MapperParams
{
    int channel = 1;
    bool faceChannels = false;      // face n plays on channel + n
    std::array<SlotParams, 4> slots {};
    bool trigger = true;            // an open mouth plays a note
    int triggerNote = 48;
    float threshold = 0.25f;        // of Open
    bool velocityFromSpeed = true;  // a quick open plays louder
    int velocity = 100;
    int pitchMode = PitchChord;
    int chord = 1;                  // kChords: the faces share it out, face 1 the root
    int scale = 2;                  // kScales: Head height picks a note of it, two octaves from the note
};

struct NamedSet { const char* name; std::vector<int> steps; };
extern const std::vector<NamedSet> kChords, kScales;

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

    // One audio block. Faces not present hold their outputs and release their notes.
    void process (const Faces&, double blockSeconds, std::vector<MidiOut>& out);
    void allOff (std::vector<MidiOut>& out);

    float slotValue (int i) const { return value[(size_t) i]; }    // smoothed, 0..1, before range
    float slotOutput (int i) const { return output[(size_t) i]; }  // what is sent, 0..1
    bool noteSounding (int face) const { return voices[(size_t) face].note >= 0; }
    int noteOf (int face) const { return voices[(size_t) face].note; }
    float gate (int face) const { return voices[(size_t) face].gate; }   // the open amount while a note sounds

    int pitchFor (int face, const Shapes&);   // the note this face plays now (Head height keeps a little hysteresis)

private:
    struct Voice { int note = -1, channel = 1, degree = -1; double armTime = 0; float gate = 0; };
    void noteOn (int face, int note, int velocity, std::vector<MidiOut>& out);
    void noteOff (int face, std::vector<MidiOut>& out);

    MapperParams params;
    std::array<float, 4> value {}, output {};
    std::array<int, 4> lastSent { -1, -1, -1, -1 };
    std::array<bool, 4> primed {};
    std::array<Voice, kMaxFaces> voices {};
    std::array<uint8_t, 16 * 128> held {};    // faces holding each channel / note, so two on one note do not cut each other off
};

} // namespace dy::mouth
