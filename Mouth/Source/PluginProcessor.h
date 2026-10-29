#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_video/juce_video.h>
#include "Engine/FaceTracker.h"
#include "Engine/MouthMapper.h"
#include <array>
#include <mutex>

namespace dy::mouth {

namespace PID
{
    inline const juce::String channel = "channel", trigger = "trigger", note = "note", threshold = "threshold",
        speed = "speed", velocity = "velocity", preview = "preview", previewLevel = "previewLevel",
        faces = "faces", pitchMode = "pitchMode", chord = "chord", scale = "scale", faceChannels = "faceChannels";
    inline juce::String slot (int i, const char* what) { return "out" + juce::String (i + 1) + what; }   // Src Dest CC Smooth Lo Hi Inv Face
}

// One colour per face number, on the picture, the meters and the outputs.
juce::Colour faceColour (int face);
juce::StringArray sourceNames();

// The webcam and the face tracker. Frames arrive on the camera's thread; the newest
// one is tracked on our own thread, so a slow frame never holds up the camera (or
// the audio, which only reads the results).
class FaceInput : private juce::CameraDevice::Listener, private juce::Thread
{
public:
    FaceInput();
    ~FaceInput() override;

    void open (int deviceIndex);            // message thread
    void close();
    int deviceIndex() const { return device != nullptr || testImage.isValid() ? openIndex : -1; }

    // results, for any thread
    std::array<std::atomic<bool>, kMaxFaces> present {};
    std::array<std::array<std::atomic<float>, kNumSources>, kMaxFaces> shape {};   // normalised 0..1
    std::atomic<float> fps { 0.0f };
    std::atomic<int> maxFaces { kMaxFaces };
    Faces faces() const;
    int faceCount() const;

    struct FaceView { int id; std::array<Point, Mesh::kCount> points; };
    struct View { juce::Image image; std::vector<FaceView> faces; juce::String status; };
    View view() const;                      // the last frame and its landmarks, for the editor

    void captureNeutral() { for (auto& n : neutralLeft) n = kNeutralFrames; }   // every face in view
    Calibration calibration (int face) const;
    void setCalibration (int face, const Calibration&);

private:
    void imageReceived (const juce::Image&) override;
    void run() override;
    void setStatus (const juce::String&);

    std::unique_ptr<juce::CameraDevice> device;
    juce::Image testImage;                  // DYMOUTH_TEST_IMAGE: a still picture instead of the camera (tests)
    int openIndex = -1;
    FaceTracker tracker;
    bool triedLoading = false;

    mutable std::mutex lock;                // pending, shown, calib, status
    juce::Image pending;
    View shown;
    std::array<Calibration, kMaxFaces> calib {};
    std::vector<unsigned char> rgb;
    static constexpr int kNeutralFrames = 15;
    std::array<std::atomic<int>, kMaxFaces> neutralLeft {};
    std::array<std::atomic<bool>, kMaxFaces> calibrated {};   // never set: a face's first frames become its neutral
    std::array<MouthShape, kMaxFaces> neutralSum {};
    int frames = 0;
    double fpsStart = 0;
};

// A small built-in voice so the plugin is audible on its own: a buzz through two
// vowel formants that follow the mouth (a talk box), one per face, placed in the
// stereo field where the face is, a head tilt bending it. Off to play another instrument.
class PreviewVoice
{
public:
    void prepare (double sampleRate) { *this = PreviewVoice(); sr = sampleRate; }
    // note < 0: silent (it fades out on its last pitch)
    void render (float* L, float* R, int n, int note, float gate, const Shapes& shape, float level);
private:
    struct Bandpass { float ic1 = 0, ic2 = 0; float process (float x, float f, float q, double sr); };
    double sr = 48000.0, phase = 0.0;
    float amp = 0, f1 = 500, f2 = 1500, freq = 110, pan = 0.5f, lastNote = 48;
    Bandpass b1, b2, b3;
};

class MouthProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    MouthProcessor();
    ~MouthProcessor() override;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void startCamera();                     // message thread; opens the chosen camera if it is not open
    void chooseCamera (const juce::String& name);
    juce::String cameraName;                // what the user picked ("" = the first)

    MapperParams readParams() const;
    juce::AudioProcessorValueTreeState apvts;
    FaceInput input;
    float uiScale = 0.85f;
    bool mirror = true;

    // for the editor
    std::array<std::atomic<float>, 4> slotOut {};
    std::array<std::atomic<int>, kMaxFaces> noteOf {};      // -1 = silent

private:
    void handleAsyncUpdate() override { startCamera(); }

    MouthMapper mapper;
    std::array<PreviewVoice, kMaxFaces> voices;
    std::vector<MidiOut> events;
    std::vector<std::atomic<float>*> raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MouthProcessor)
};

} // namespace dy::mouth
