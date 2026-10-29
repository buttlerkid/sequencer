#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace dy::mouth {

juce::Colour faceColour (int who)
{
    static const juce::Colour c[kNumWho] { Colours::brass, Colours::teal, Colours::lilac, juce::Colour (0xffe8897a),
                                           juce::Colour (0xff8fd18a), juce::Colour (0xfff0d264) };
    return c[(size_t) juce::jlimit (0, kNumWho - 1, who)];
}

juce::StringArray whoNames() { return { "Face 1", "Face 2", "Face 3", "Face 4", "Hand 1", "Hand 2" }; }

// ------------------------------------------------------------------ camera + tracker
FaceInput::FaceInput() : juce::Thread ("DY Mouth tracker") {}

FaceInput::~FaceInput()
{
    close();
    handThread.stopThread (3000);
    stopThread (3000);
}

void FaceInput::open (int index)
{
    close();
    if (const auto test = juce::SystemStats::getEnvironmentVariable ("DYMOUTH_TEST_IMAGE", {}); test.isNotEmpty())
    {
        testImage = juce::ImageFileFormat::loadFrom (juce::File (test));
        openIndex = index;
        if (! isThreadRunning()) startThread();
        return;
    }
    device.reset (juce::CameraDevice::openDevice (index, 320, 240, 1280, 720));
    if (device == nullptr)
    {
        setStatus ("Camera not available: is another app using it? (Windows: Settings > Privacy > Camera)");
        return;
    }
    openIndex = index;
    setStatus ("Looking for a face");
    device->addListener (this);
    if (! isThreadRunning()) startThread();
}

void FaceInput::close()
{
    if (device != nullptr) device->removeListener (this);
    device.reset();
    openIndex = -1;
    for (auto& p : present) p = false;
    for (auto& p : handPresent) p = false;
}

void FaceInput::imageReceived (const juce::Image& img)
{
    {
        std::lock_guard<std::mutex> g (lock);
        pending = img;
    }
    notify();
}

void FaceInput::setStatus (const juce::String& s)
{
    std::lock_guard<std::mutex> g (lock);
    shown.status = s;
}

FaceInput::View FaceInput::view() const
{
    std::lock_guard<std::mutex> g (lock);
    return shown;
}

Scene FaceInput::scene() const
{
    Scene f;
    for (size_t i = 0; i < (size_t) kMaxFaces; ++i)
    {
        f.present[i] = present[i].load();
        for (size_t k = 0; k < (size_t) kNumSources; ++k) f.shapes[i][k] = shape[i][k].load();
    }
    for (size_t h = 0; h < (size_t) kMaxHands; ++h)
    {
        const auto w = (size_t) handWho ((int) h);
        f.present[w] = handPresent[h].load();
        for (size_t k = 0; k < (size_t) kNumSources; ++k) f.shapes[w][k] = handShape[h][k].load();
        f.circles[h] = circles[h].load();
        f.circleSize[h] = circleSize[h].load();
        f.circleSpeed[h] = circleSpeed[h].load();
    }
    return f;
}

int FaceInput::handCount() const
{
    int n = 0;
    for (auto& p : handPresent) n += p.load() ? 1 : 0;
    return n;
}

int FaceInput::faceCount() const
{
    int n = 0;
    for (auto& p : present) n += p.load() ? 1 : 0;
    return n;
}

Calibration FaceInput::calibration (int face) const
{
    std::lock_guard<std::mutex> g (lock);
    return calib[(size_t) face];
}

void FaceInput::setCalibration (int face, const Calibration& c)
{
    std::lock_guard<std::mutex> g (lock);
    calib[(size_t) face] = c;
    calibrated[(size_t) face] = true;
}

void FaceInput::run()
{
    while (! threadShouldExit())
    {
        wait (testImage.isValid() ? 33 : 200);
        if (! triedLoading)
        {
            triedLoading = true;
            const auto dll = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("DYMouth.onnxruntime.dll");
            if (! tracker.load (dll.getFullPathName().toWideCharPointer(),
                                MouthModels::face_detector_onnx, (size_t) MouthModels::face_detector_onnxSize,
                                MouthModels::face_landmarks_detector_onnx, (size_t) MouthModels::face_landmarks_detector_onnxSize))
                setStatus (tracker.error());
            else if (hands.load (dll.getFullPathName().toWideCharPointer(),
                                 MouthModels::hand_detector_onnx, (size_t) MouthModels::hand_detector_onnxSize,
                                 MouthModels::hand_landmarks_detector_onnx, (size_t) MouthModels::hand_landmarks_detector_onnxSize))
                handThread.startThread();
        }

        juce::Image img;
        {
            std::lock_guard<std::mutex> g (lock);
            std::swap (img, pending);
        }
        if (testImage.isValid()) img = testImage;
        if (! img.isValid() || ! tracker.isLoaded()) continue;

        // to packed RGB, at most 640 wide (the models look at 128 to 256 px squares)
        auto frame = std::make_shared<Rgb>();
        const int step = std::max (1, (img.getWidth() + 639) / 640);
        const int w = img.getWidth() / step, h = img.getHeight() / step;
        frame->w = w; frame->h = h; frame->step = step;
        frame->time = juce::Time::getMillisecondCounterHiRes() * 0.001;
        frame->data.resize ((size_t) w * (size_t) h * 3);
        {
            const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            const bool rgbFormat = bd.pixelFormat == juce::Image::RGB, argb = bd.pixelFormat == juce::Image::ARGB;
            for (int y = 0; y < h; ++y)
            {
                unsigned char* d = frame->data.data() + (size_t) y * (size_t) w * 3;
                const juce::uint8* row = bd.getLinePointer (y * step);
                for (int x = 0; x < w; ++x, d += 3)
                {
                    const juce::uint8* px = row + (size_t) (x * step) * (size_t) bd.pixelStride;
                    if (rgbFormat)   { const auto* q = reinterpret_cast<const juce::PixelRGB*> (px);  d[0] = q->getRed(); d[1] = q->getGreen(); d[2] = q->getBlue(); }
                    else if (argb)   { const auto* q = reinterpret_cast<const juce::PixelARGB*> (px); d[0] = q->getRed(); d[1] = q->getGreen(); d[2] = q->getBlue(); }
                    else             { d[0] = d[1] = d[2] = *px; }
                }
            }
        }
        // the hands follow the same frame on their own thread, at the same time
        {
            std::lock_guard<std::mutex> g (lock);
            handFrame = frame;
        }
        handThread.notify();

        const float dt = lastFrameTime > 0 ? (float) juce::jlimit (0.0, 0.5, frame->time - lastFrameTime) : 0.033f;
        lastFrameTime = frame->time;
        tracker.setMax (maxFaces.load());
        auto found = tracker.track ({ frame->data.data(), w, h, w * 3 });
        std::array<bool, kMaxFaces> seen {};
        std::vector<FaceView> views;
        for (auto& r : found)
        {
            const auto id = (size_t) r.id;
            for (auto& p : r.points) p = { p.x * (float) step, p.y * (float) step };
            const auto s = measure (r.points.data(), (float) img.getWidth(), (float) img.getHeight());
            if (! calibrated[id]) { calibrated[id] = true; neutralLeft[id] = kNeutralFrames; }
            Calibration c = calibration (r.id);
            if (int left = neutralLeft[id].load(); left > 0)
            {
                auto& sum = neutralSum[id];
                if (left == kNeutralFrames) sum = {};
                sum.gap += s.gap; sum.width += s.width; sum.lift += s.lift; sum.brow += s.brow; sum.roll += s.roll;
                if (--left == 0)
                {
                    const float n = (float) kNeutralFrames;
                    c.gapRest = sum.gap / n; c.widthRest = sum.width / n; c.liftRest = sum.lift / n;
                    c.browRest = sum.brow / n; c.rollRest = sum.roll / n;
                    setCalibration (r.id, c);
                }
                neutralLeft[id] = left;
            }
            const auto v = normalise (s, c, &autoRange[id], dt);
            for (size_t k = 0; k < v.size(); ++k) shape[id][k] = v[k];
            seen[id] = true;
            views.push_back ({ r.id, r.points });
        }
        for (size_t i = 0; i < (size_t) kMaxFaces; ++i)
        {
            present[i] = seen[i];
            if (! seen[i] && neutralLeft[i].load() != kNeutralFrames) neutralLeft[i] = 0;   // a face that leaves half way drops its Neutral
        }

        {
            std::lock_guard<std::mutex> g (lock);
            shown.image = img;
            shown.faces = std::move (views);
            shown.hands = shownHands;
            shown.status = ! found.empty() || ! shownHands.empty() ? juce::String()
                         : (tracker.error().empty() ? juce::String ("Looking for faces and hands") : juce::String (tracker.error()));
        }

        ++frames;
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - fpsStart > 1000.0)
        {
            fps = (float) (frames * 1000.0 / (now - fpsStart));
            frames = 0;
            fpsStart = now;
        }
    }
}

void FaceInput::trackHands()
{
    std::shared_ptr<const Rgb> f;
    {
        std::lock_guard<std::mutex> g (lock);
        std::swap (f, handFrame);
    }
    if (f == nullptr) return;
    hands.setMax (maxHands.load());
    const auto found = hands.track ({ f->data.data(), f->w, f->h, f->w * 3 });
    std::array<bool, kMaxHands> seen {};
    std::vector<HandView> views;
    for (auto r : found)
    {
        const auto id = (size_t) r.id;
        for (auto& p : r.points) p = { p.x * (float) f->step, p.y * (float) f->step };
        const float W = (float) (f->w * f->step), H = (float) (f->h * f->step);
        const auto s = measureHand (r.points.data(), W, H);
        auto& det = circleDetectors[id];
        if (! handPresent[id].load()) det.reset();      // a hand that came back starts a new path
        CircleDetector::Event e;
        if (det.add (s.palm, f->time, H, e))
        {
            circleSize[id] = e.size;
            circleSpeed[id] = e.speed;
            circles[id] = circles[id].load() + 1;       // the audio thread plays it
            lastCircleTime[id] = f->time;
        }
        auto& out = handShape[id];
        out[HandX] = s.x; out[HandY] = s.y; out[Pinch] = s.pinch; out[Spread] = s.spread;
        out[Circle] = det.circle(); out[Spin] = det.spin();
        seen[id] = true;
        HandView v { r.id, r.points, {}, det.circle(), f->time - lastCircleTime[id] };
        for (auto& q : det.trail()) v.trail.push_back (q.first);
        views.push_back (std::move (v));
    }
    for (size_t i = 0; i < (size_t) kMaxHands; ++i) handPresent[i] = seen[i];
    std::lock_guard<std::mutex> g (lock);
    shownHands = std::move (views);
}

// ------------------------------------------------------------------ bell + recorder
void PreviewBell::noteOn (int note, float velocity, float pan)
{
    auto& v = voices[(size_t) next];
    next = (next + 1) % (int) voices.size();
    const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    v.inc1 = f / sr;
    v.inc2 = f * 2.76 / sr;          // a bell's inharmonic partial
    v.ph1 = v.ph2 = 0.0;
    v.amp = 0.35f * velocity;
    v.decay = (float) std::exp (-1.0 / (sr * (0.6 + 1.2 * juce::jlimit (0.0, 1.0, 1.0 - (note - 24) / 72.0))));   // low notes ring longer
    v.pan = pan;
}

void PreviewBell::render (float* L, float* R, int n, float level)
{
    for (auto& v : voices)
    {
        if (v.amp < 1.0e-4f) continue;
        const float gl = std::sqrt (1.0f - 0.7f * v.pan) * level, gr = std::sqrt (0.3f + 0.7f * v.pan) * level;
        for (int i = 0; i < n; ++i)
        {
            // the partial fades faster than the fundamental: a strike, then a ring
            const float x = v.amp * (float) (std::sin (v.ph1 * juce::MathConstants<double>::twoPi)
                                             + 1.2 * v.amp * std::sin (v.ph2 * juce::MathConstants<double>::twoPi));
            v.ph1 += v.inc1; if (v.ph1 >= 1.0) v.ph1 -= 1.0;
            v.ph2 += v.inc2; if (v.ph2 >= 1.0) v.ph2 -= 1.0;
            v.amp *= v.decay;
            L[i] += x * gl;
            if (R != L) R[i] += x * gr;
        }
    }
}

void Recorder::start (double bpmNow)
{
    const juce::SpinLock::ScopedLockType g (lock);
    take.clear();
    tempo = bpmNow;
    seconds = 0.0;
    noteCount = 0;
    recording = true;
}

void Recorder::stop()
{
    const juce::SpinLock::ScopedLockType g (lock);
    recording = false;
    take.updateMatchedPairs();
    // notes still held end where the take ends
    juce::MidiMessageSequence ends;
    for (int i = 0; i < take.getNumEvents(); ++i)
        if (auto* e = take.getEventPointer (i); e->message.isNoteOn() && e->noteOffObject == nullptr)
            ends.addEvent (juce::MidiMessage::noteOff (e->message.getChannel(), e->message.getNoteNumber()), seconds.load());
    take.addSequence (ends, 0.0);
    take.updateMatchedPairs();
}

void Recorder::add (const juce::MidiMessage& m, double t)
{
    const juce::SpinLock::ScopedTryLockType g (lock);
    if (! g.isLocked() || ! recording.load()) return;
    take.addEvent (m, t);
    seconds = t;
    if (m.isNoteOn()) noteCount = noteCount.load() + 1;
}

bool Recorder::writeTo (const juce::File& file) const
{
    constexpr int ppq = 960;
    juce::MidiMessageSequence track;
    double bpmOfTake = 120.0;
    {
        const juce::SpinLock::ScopedLockType g (lock);
        track = take;
        bpmOfTake = tempo;
    }
    if (track.getNumEvents() == 0) return false;
    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        auto& m = track.getEventPointer (i)->message;
        m.setTimeStamp (m.getTimeStamp() * bpmOfTake / 60.0 * ppq);      // seconds -> ticks
    }
    track.addEvent (juce::MidiMessage::endOfTrack(), track.getEndTime());
    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpmOfTake)), 0.0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (meta);
    mf.addTrack (track);
    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && mf.writeTo (out);
}

// ------------------------------------------------------------------ preview voice
float PreviewVoice::Bandpass::process (float x, float f, float q, double rate)
{
    const float g = std::tan (juce::MathConstants<float>::pi * std::min (f, (float) rate * 0.45f) / (float) rate);
    const float k = 1.0f / q, a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return v1;
}

void PreviewVoice::render (float* L, float* R, int n, int note, float gate, const Shapes& s, float level)
{
    if (note >= 0) lastNote = (float) note;
    const float targetAmp = note >= 0 ? 0.45f + 0.55f * gate : 0.0f;
    if (targetAmp == 0.0f && amp < 1.0e-4f) { amp = 0.0f; return; }
    const float t1 = 280.0f + 620.0f * s[Open];
    const float t2 = juce::jlimit (600.0f, 2600.0f, 950.0f + 1300.0f * s[Width] + 500.0f * s[Smile] - 450.0f * s[Pucker]);
    // a head tilt bends it up to two semitones either way (a little dead zone for a straight head)
    const float tilt = (s[Tilt] - 0.5f) * 2.0f;
    const float bend = 2.0f * (std::abs (tilt) < 0.15f ? 0.0f : (tilt - std::copysign (0.15f, tilt)) / 0.85f);
    const float targetFreq = 440.0f * std::pow (2.0f, (lastNote + bend - 69.0f) / 12.0f);
    const float glide = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    const float targetPan = s[HeadX];
    // raised brows sing with vibrato (up to half a semitone, 5.5 times a second)
    const double vibDepth = 0.5 * s[Brows], vibInc = 5.5 / sr;
    for (int i = 0; i < n; ++i)
    {
        pan += (targetPan - pan) * glide;
        vibPhase += vibInc;
        if (vibPhase >= 1.0) vibPhase -= 1.0;
        const double vib = std::pow (2.0, vibDepth * std::sin (vibPhase * juce::MathConstants<double>::twoPi) / 12.0);
        amp += (targetAmp - amp) * glide;
        f1 += (t1 - f1) * glide;
        f2 += (t2 - f2) * glide;
        freq += (targetFreq - freq) * glide;
        if (amp < 1.0e-4f && targetAmp == 0.0f) { amp = 0.0f; continue; }

        // a band-limited saw (polyBLEP)
        const double dt = freq * vib / sr;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        auto blep = [dt] (double t) { if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
                                      if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; } return 0.0; };
        const float saw = (float) (2.0 * phase - 1.0 - blep (phase));

        const float y = b1.process (saw, f1, 7.0f, sr) + 0.75f * b2.process (saw, f2, 9.0f, sr)
                      + (0.25f + 0.5f * s[Smile]) * b3.process (saw, 2700.0f, 8.0f, sr);   // a smile is brighter
        const float out = y * amp * level * 0.6f;
        if (R == L) { L[i] += out; continue; }
        L[i] += out * std::sqrt (1.0f - 0.7f * pan);
        R[i] += out * std::sqrt (0.3f + 0.7f * pan);
    }
}

// ------------------------------------------------------------------ parameters
juce::StringArray sourceNames() { return { "Open", "Width", "Smile", "Pucker", "Brows", "Tilt", "Head X", "Head Y",
                                            "Hand X", "Hand Y", "Pinch", "Spread", "Circle", "Spin" }; }

static juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const String& s) { return ParameterID { s, 1 }; };
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto noteName = AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 3); });
    StringArray chans;
    for (int c = 1; c <= 16; ++c) chans.add (String (c));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::channel), "MIDI Channel", chans, 0));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::trigger), "Note On Open", true));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::note), "Note", 24, 96, 48, noteName));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::threshold), "Threshold", NormalisableRange<float> (0.05f, 0.9f), 0.25f, pct));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::speed), "Velocity From Speed", true));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::velocity), "Velocity", 1, 127, 100));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::preview), "Preview Sound", true));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::previewLevel), "Preview Level", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::faces), "Faces", StringArray { "1 face", "2 faces", "3 faces", "4 faces" }, kMaxFaces - 1));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::pitchMode), "Pitch", StringArray { "Chord", "Head height" }, PitchChord));
    StringArray chords, scales;
    for (auto& c : kChords) chords.add (c.name);
    for (auto& c : kScales) scales.add (c.name);
    l.add (std::make_unique<AudioParameterChoice> (id (PID::chord), "Chord", chords, 1));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::scale), "Scale", scales, 2));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::faceChannels), "Face = Channel", false));
    l.add (std::make_unique<AudioParameterChoice> (id (PID::hands), "Hands", StringArray { "Off", "1 hand", "2 hands" }, kMaxHands));
    l.add (std::make_unique<AudioParameterBool>   (id (PID::circleNotes), "Circle Notes", true));
    l.add (std::make_unique<AudioParameterInt>    (id (PID::circleNote), "Circle Note", 24, 96, 48, noteName));
    l.add (std::make_unique<AudioParameterFloat>  (id (PID::circleLength), "Circle Note Length", NormalisableRange<float> (0.05f, 2.0f, 0.0f, 0.5f), 0.4f,
                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })));

    // defaults: face 1 open -> mod wheel, face 1 tilt -> pitch bend, hand 1 circle -> CC 74, hand 1 height -> CC 11
    const int src[4] = { Open, Tilt, Circle, HandY }, dest[4] = { DestCC, DestPitchBend, DestCC, DestCC }, cc[4] = { 1, 2, 74, 11 };
    const int who[4] = { 0, 0, handWho (0), handWho (0) };
    for (int i = 0; i < 4; ++i)
    {
        const String n = "Out " + String (i + 1) + " ";
        l.add (std::make_unique<AudioParameterChoice> (id (PID::slot (i, "Src")), n + "Source", sourceNames(), src[i]));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::slot (i, "Dest")), n + "Send", StringArray { "Off", "CC", "Pitch bend", "Pressure" }, dest[i]));
        l.add (std::make_unique<AudioParameterInt>    (id (PID::slot (i, "CC")), n + "CC", 0, 127, cc[i],
                                                       AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                       { return v == 1 ? String ("1 mod") : v == 11 ? String ("11 expr") : v == 74 ? String ("74 bright") : String (v); })));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Smooth")), n + "Smooth", NormalisableRange<float> (0.0f, 1000.0f, 0.0f, 0.4f), 60.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Lo")), n + "Min", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Hi")), n + "Max", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
        l.add (std::make_unique<AudioParameterBool>   (id (PID::slot (i, "Inv")), n + "Invert", false));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::slot (i, "Face")), n + "Who", whoNames(), who[i]));
    }
    return l;
}

static juce::StringArray paramIds()
{
    juce::StringArray ids { PID::channel, PID::trigger, PID::note, PID::threshold, PID::speed, PID::velocity, PID::preview, PID::previewLevel,
                            PID::faces, PID::pitchMode, PID::chord, PID::scale, PID::faceChannels,
                            PID::hands, PID::circleNotes, PID::circleNote, PID::circleLength };
    for (int i = 0; i < 4; ++i)
        for (auto* w : { "Src", "Dest", "CC", "Smooth", "Lo", "Hi", "Inv", "Face" }) ids.add (PID::slot (i, w));
    return ids;
}

MouthProcessor::MouthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", layout())
{
    for (auto& id : paramIds()) raw.push_back (apvts.getRawParameterValue (id));
}

MouthProcessor::~MouthProcessor()
{
    cancelPendingUpdate();
    input.close();
}

MapperParams MouthProcessor::readParams() const
{
    auto f = [&] (int k) { return raw[(size_t) k]->load(); };
    auto i = [&] (int k) { return (int) std::lround (f (k)); };
    MapperParams p;
    p.channel = i (0) + 1;
    p.trigger = f (1) > 0.5f; p.triggerNote = i (2); p.threshold = f (3);
    p.velocityFromSpeed = f (4) > 0.5f; p.velocity = i (5);
    p.pitchMode = i (9); p.chord = i (10); p.scale = i (11); p.faceChannels = f (12) > 0.5f;
    p.circleNotes = f (14) > 0.5f; p.circleNote = i (15); p.circleLength = f (16);
    for (int s = 0; s < 4; ++s)
    {
        const int b = 17 + s * 8;
        p.slots[(size_t) s] = { i (b), i (b + 1), i (b + 2), f (b + 3), f (b + 4), f (b + 5), f (b + 6) > 0.5f, i (b + 7) };
    }
    return p;
}

void MouthProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& v : voices) v.prepare (sampleRate);
    bell.prepare (sampleRate);
    triggerAsyncUpdate();             // the camera opens once we are really in use, not while a host scans
}

void MouthProcessor::startCamera()
{
    if (input.deviceIndex() >= 0) return;
    const auto devices = juce::CameraDevice::getAvailableDevices();
    if (devices.isEmpty()) return;
    input.open (std::max (0, devices.indexOf (cameraName)));
}

void MouthProcessor::chooseCamera (const juce::String& name)
{
    cameraName = name;
    input.close();
    startCamera();
}

bool MouthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void MouthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();                                    // the mouth is the only source of MIDI

    input.maxFaces = (int) raw[8]->load() + 1;
    input.maxHands = (int) raw[13]->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    const auto faces = input.scene();
    mapper.setParams (readParams());
    events.clear();
    mapper.process (faces, n / getSampleRate(), events);

    // the take: started and stopped here, so its clock is the audio's
    if (recordStart.exchange (false)) { recorder.start (bpm); recordSamples = 0; }
    const double takeTime = recordSamples / getSampleRate();
    recordSamples += n;

    const bool preview = raw[6]->load() > 0.5f;
    for (const auto& e : events)
    {
        juce::MidiMessage m;
        switch (e.type)
        {
            case MidiOut::NoteOn:    m = juce::MidiMessage::noteOn (e.channel, e.data1, (juce::uint8) e.data2); break;
            case MidiOut::NoteOff:   m = juce::MidiMessage::noteOff (e.channel, e.data1); break;
            case MidiOut::CC:        m = juce::MidiMessage::controllerEvent (e.channel, e.data1, e.data2); break;
            case MidiOut::PitchBend: m = juce::MidiMessage::pitchWheel (e.channel, e.data1); break;
            case MidiOut::Pressure:  m = juce::MidiMessage::channelPressureChange (e.channel, e.data1); break;
        }
        midi.addEvent (m, 0);
        recorder.add (m, takeTime);
        // a hand's circle rings the bell, where the hand is
        if (preview && e.type == MidiOut::NoteOn && e.who >= kMaxFaces)
            bell.noteOn (e.data1, e.data2 / 127.0f, faces.shapes[(size_t) e.who][HandX]);
    }

    for (size_t i = 0; i < slotOut.size(); ++i) slotOut[i] = mapper.slotOutput ((int) i);
    for (int f = 0; f < kMaxFaces; ++f) noteOf[(size_t) f] = mapper.noteOf (f);

    if (preview && buffer.getNumChannels() > 0)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (buffer.getNumChannels() > 1 ? 1 : 0);
        for (int f = 0; f < kMaxFaces; ++f)
            voices[(size_t) f].render (L, R, n, mapper.noteOf (f), mapper.gate (f), faces.shapes[(size_t) f], raw[7]->load() * 0.8f);
        bell.render (L, R, n, raw[7]->load());
    }
}

void MouthProcessor::setRecording (bool on)
{
    if (on) recordStart = true;
    else if (recorder.isRecording()) recorder.stop();
}

void MouthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("DYMouth");
    root.setProperty ("version", 2, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("mirror", mirror, nullptr);
    root.setProperty ("camera", cameraName, nullptr);
    for (int f = 0; f < kMaxFaces; ++f)
    {
        const auto c = input.calibration (f);
        juce::ValueTree t ("Neutral");
        t.setProperty ("gap", c.gapRest, nullptr);
        t.setProperty ("width", c.widthRest, nullptr);
        t.setProperty ("lift", c.liftRest, nullptr);
        t.setProperty ("brow", c.browRest, nullptr);
        t.setProperty ("roll", c.rollRest, nullptr);
        root.addChild (t, -1, nullptr);
    }
    root.addChild (apvts.copyState(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void MouthProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid()) apvts.replaceState (p);
    uiScale = (float) (double) root.getProperty ("uiScale", 0.85);
    mirror = (bool) root.getProperty ("mirror", true);
    cameraName = root.getProperty ("camera", "").toString();
    // each face's neutral (a 0.1 project has none: its faces calibrate when first seen)
    int f = 0;
    for (const auto& t : root)
    {
        if (! t.hasType ("Neutral") || f >= kMaxFaces) continue;
        Calibration c;
        c.gapRest = (float) (double) t.getProperty ("gap", c.gapRest);
        c.widthRest = (float) (double) t.getProperty ("width", c.widthRest);
        c.liftRest = (float) (double) t.getProperty ("lift", c.liftRest);
        c.browRest = (float) (double) t.getProperty ("brow", c.browRest);
        c.rollRest = (float) (double) t.getProperty ("roll", c.rollRest);
        input.setCalibration (f++, c);
    }
}

juce::AudioProcessorEditor* MouthProcessor::createEditor() { return new MouthEditor (*this); }

} // namespace dy::mouth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::mouth::MouthProcessor(); }
