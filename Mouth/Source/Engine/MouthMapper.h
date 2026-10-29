#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <vector>

// DY Mouth engine, the part with no camera and no JUCE: face and hand landmarks ->
// shapes and gestures -> MIDI. Kept apart so it is unit tested (Tests/MouthTests.cpp).

namespace dy::mouth {

constexpr int kMaxFaces = 4, kMaxHands = 2;
constexpr int kNumWho = kMaxFaces + kMaxHands;     // faces 0..3, then hands 4..5
constexpr int handWho (int hand) { return kMaxFaces + hand; }

// What the plugin follows, all 0..1. Faces fill Open .. HeadY, hands HandX .. Spin.
enum Source { Open = 0, Width, Smile, Pucker, Brows, Tilt, HeadX, HeadY,
              HandX, HandY, Pinch, Spread, Circle, Spin, kNumSources };
constexpr bool isHandSource (int s) { return s >= HandX; }
using Shapes = std::array<float, kNumSources>;

// Everyone in view this frame, by number (kept while they stay in view).
struct Scene
{
    std::array<Shapes, kNumWho> shapes {};
    std::array<bool, kNumWho> present {};
    // circles drawn by each hand so far, and the last one's size and speed (0..1)
    std::array<int, kMaxHands> circles {};
    std::array<float, kMaxHands> circleSize {}, circleSpeed {};
};
using Faces = Scene;

struct Point { float x = 0, y = 0; };

// ------------------------------------------------------------------ faces
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

// Every face moves differently: each shape remembers the furthest this face has
// taken it (forgetting slowly), and that is 100 %. A floor keeps small twitches small.
struct AutoRange
{
    std::array<float, 5> peak {};   // Open, Width, Smile, Pucker, Brows
    void reset() { peak = {}; }
};

// autoRange (optional) learns as it goes; dt is the time since the last frame.
Shapes normalise (const MouthShape&, const Calibration&, AutoRange* autoRange = nullptr, float dt = 0.0f);

// ------------------------------------------------------------------ hands
namespace HandMesh
{
    constexpr int kCount = 21;
    constexpr int wrist = 0, thumbTip = 4, indexKnuckle = 5, indexTip = 8, middleKnuckle = 9, middleTip = 12,
                  ringKnuckle = 13, ringTip = 16, pinkyKnuckle = 17, pinkyTip = 20;
    // the bones, for drawing
    constexpr std::array<std::array<int, 2>, 21> bones { { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 0, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 },
                                                           { 5, 9 }, { 9, 10 }, { 10, 11 }, { 11, 12 }, { 9, 13 }, { 13, 14 }, { 14, 15 },
                                                           { 15, 16 }, { 13, 17 }, { 17, 18 }, { 18, 19 }, { 19, 20 }, { 0, 17 } } };
}

struct HandShape
{
    Point palm;                     // frame pixels
    float x = 0.5f, y = 0.5f;       // palm in the frame as the person sees it (mirrored, up = 1)
    float pinch = 0;                // thumb and index tips together = 1
    float spread = 0;               // fist 0, open hand 1
};

HandShape measureHand (const Point* landmarks, float frameWidth, float frameHeight);

// Circles drawn with a hand: follows the palm, counts the turns around the middle of
// its recent path, and reports each full turn with its size and speed.
class CircleDetector
{
public:
    struct Event { float size, speed; int direction; };   // size, speed 0..1; +1 clockwise as the person sees it
    // p in frame pixels, t in seconds; true when a circle has just been completed
    bool add (Point p, double t, float frameHeight, Event& event);
    void reset();
    float circle() const { return level; }      // the size of the circle being drawn now, 0 when not circling
    float spin() const { return spinLevel; }    // 0.5 still, towards 1 / 0 the faster one way / the other
    const std::deque<std::pair<Point, double>>& trail() const { return path; }

private:
    std::deque<std::pair<Point, double>> path;
    double turn = 0, lastT = -1;
    float lastAngle = 0, omega = 0, level = 0, spinLevel = 0.5f;
    bool haveAngle = false;
};

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
    int who = 0;                    // face 0..3, hand 4..5
};

struct MapperParams
{
    int channel = 1;
    bool faceChannels = false;      // face n plays on channel + n, hand n on channel + 4 + n
    std::array<SlotParams, 4> slots {};
    bool trigger = true;            // an open mouth plays a note
    int triggerNote = 48;
    float threshold = 0.25f;        // of Open
    bool velocityFromSpeed = true;  // a quick open plays louder
    int velocity = 100;
    int pitchMode = PitchChord;
    int chord = 1;                  // kChords: the faces share it out, face 1 the root
    int scale = 2;                  // kScales: Head height picks a note of it, two octaves from the note
    bool circleNotes = true;        // every circle a hand draws plays a note
    int circleNote = 48;            // the biggest circle; smaller ones climb the scale, two octaves
    float circleLength = 0.4f;      // seconds
};

struct NamedSet { const char* name; std::vector<int> steps; };
extern const std::vector<NamedSet> kChords, kScales;

struct MidiOut
{
    enum Type { NoteOn, NoteOff, CC, PitchBend, Pressure } type;
    int channel, data1, data2;      // PitchBend: data1 = 0..16383
    int who = -1;                   // the face or hand that played a note
};

class MouthMapper
{
public:
    void setParams (const MapperParams&);
    const MapperParams& getParams() const { return params; }

    // One audio block. Anyone not present holds their outputs and releases their notes.
    void process (const Scene&, double blockSeconds, std::vector<MidiOut>& out);
    void allOff (std::vector<MidiOut>& out);

    float slotValue (int i) const { return value[(size_t) i]; }    // smoothed, 0..1, before range
    float slotOutput (int i) const { return output[(size_t) i]; }  // what is sent, 0..1
    bool noteSounding (int face) const { return voices[(size_t) face].note >= 0; }
    int noteOf (int face) const { return voices[(size_t) face].note; }
    float gate (int face) const { return voices[(size_t) face].gate; }   // the open amount while a note sounds

    int pitchFor (int face, const Shapes&);   // the note this face plays now (Head height keeps a little hysteresis)
    int circlePitch (float size) const;       // the note a circle of this size plays

private:
    struct Voice { int note = -1, channel = 1, degree = -1; double armTime = 0; float gate = 0; };
    struct Pending { int channel, note; double left; };
    void noteOn (int face, int note, int velocity, std::vector<MidiOut>& out);
    void noteOff (int face, std::vector<MidiOut>& out);
    bool press (int channel, int note, int velocity, int who, std::vector<MidiOut>& out);
    void release (int channel, int note, std::vector<MidiOut>& out);

    MapperParams params;
    std::array<float, 4> value {}, output {};
    std::array<int, 4> lastSent { -1, -1, -1, -1 };
    std::array<bool, 4> primed {};
    std::array<Voice, kMaxFaces> voices {};
    std::array<int, kMaxHands> circlesSeen { -1, -1 };
    std::vector<Pending> pending;              // circle notes waiting for their note off
    std::array<uint8_t, 16 * 128> held {};    // players holding each channel / note, so two on one note do not cut each other off
};

} // namespace dy::mouth
