#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace dy::mouth {

// ------------------------------------------------------------------ camera + tracker
FaceInput::FaceInput() : juce::Thread ("DY Mouth tracker") {}

FaceInput::~FaceInput()
{
    close();
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
    face = false;
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

Calibration FaceInput::calibration() const
{
    std::lock_guard<std::mutex> g (lock);
    return calib;
}

void FaceInput::setCalibration (const Calibration& c)
{
    std::lock_guard<std::mutex> g (lock);
    calib = c;
    calibrated = true;
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
        }

        juce::Image img;
        {
            std::lock_guard<std::mutex> g (lock);
            std::swap (img, pending);
        }
        if (testImage.isValid()) img = testImage;
        if (! img.isValid() || ! tracker.isLoaded()) continue;

        // to packed RGB, at most 640 wide (the models look at 128 and 256 px squares)
        const int step = std::max (1, (img.getWidth() + 639) / 640);
        const int w = img.getWidth() / step, h = img.getHeight() / step;
        rgb.resize ((size_t) w * (size_t) h * 3);
        {
            const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < h; ++y)
            {
                unsigned char* d = rgb.data() + (size_t) y * (size_t) w * 3;
                for (int x = 0; x < w; ++x, d += 3)
                {
                    if (bd.pixelFormat == juce::Image::RGB)
                    {
                        const auto* p = reinterpret_cast<const juce::PixelRGB*> (bd.getPixelPointer (x * step, y * step));
                        d[0] = p->getRed(); d[1] = p->getGreen(); d[2] = p->getBlue();
                    }
                    else
                    {
                        const auto c = bd.getPixelColour (x * step, y * step);
                        d[0] = c.getRed(); d[1] = c.getGreen(); d[2] = c.getBlue();
                    }
                }
            }
        }

        auto res = tracker.track ({ rgb.data(), w, h, w * 3 });
        for (auto& p : res.points) p = { p.x * (float) step, p.y * (float) step };
        if (res.face)
        {
            const auto s = measure (res.points.data());
            if (! calibrated) { calibrated = true; captureNeutral(); }
            Calibration c = calibration();
            if (int left = neutralFrames.load(); left > 0)
            {
                neutralSum.gap += s.gap; neutralSum.width += s.width; neutralSum.lift += s.lift;
                if (--left == 0)
                {
                    c.gapRest = neutralSum.gap / 15.0f; c.widthRest = neutralSum.width / 15.0f; c.liftRest = neutralSum.lift / 15.0f;
                    setCalibration (c);
                }
                neutralFrames = left;
            }
            const auto v = normalise (s, c);
            for (size_t i = 0; i < shape.size(); ++i) shape[i] = v[i];
        }
        face = res.face;

        {
            std::lock_guard<std::mutex> g (lock);
            shown.image = img;
            shown.points = res.points;
            shown.face = res.face;
            shown.status = res.face ? juce::String() : (tracker.error().empty() ? juce::String ("Looking for a face") : juce::String (tracker.error()));
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

void PreviewVoice::render (float* L, float* R, int n, int note, float gate, const std::array<float, kNumSources>& s, float level)
{
    const float targetAmp = gate > 0.0f ? 0.45f + 0.55f * gate : 0.0f;
    const float t1 = 280.0f + 620.0f * s[Open];
    const float t2 = juce::jlimit (600.0f, 2600.0f, 950.0f + 1300.0f * s[Width] + 500.0f * s[Smile] - 450.0f * s[Pucker]);
    const float targetFreq = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
    const float glide = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        amp += (targetAmp - amp) * glide;
        f1 += (t1 - f1) * glide;
        f2 += (t2 - f2) * glide;
        freq += (targetFreq - freq) * glide;
        if (amp < 1.0e-4f && targetAmp == 0.0f) { amp = 0.0f; continue; }

        // a band-limited saw (polyBLEP)
        const double dt = freq / sr;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        auto blep = [dt] (double t) { if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
                                      if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; } return 0.0; };
        const float saw = (float) (2.0 * phase - 1.0 - blep (phase));

        const float y = b1.process (saw, f1, 7.0f, sr) + 0.75f * b2.process (saw, f2, 9.0f, sr) + 0.3f * b3.process (saw, 2700.0f, 8.0f, sr);
        const float out = y * amp * level * 0.6f;
        L[i] += out;
        if (R != L) R[i] += out;
    }
}

// ------------------------------------------------------------------ parameters
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

    // defaults: Open -> mod wheel, Smile -> pressure, Width -> CC 74, Pucker -> CC 71
    const int src[4] = { Open, Smile, Width, Pucker }, dest[4] = { DestCC, DestPressure, DestCC, DestCC }, cc[4] = { 1, 2, 74, 71 };
    for (int i = 0; i < 4; ++i)
    {
        const String n = "Out " + String (i + 1) + " ";
        l.add (std::make_unique<AudioParameterChoice> (id (PID::slot (i, "Src")), n + "Source", StringArray { "Open", "Width", "Smile", "Pucker" }, src[i]));
        l.add (std::make_unique<AudioParameterChoice> (id (PID::slot (i, "Dest")), n + "Send", StringArray { "Off", "CC", "Pitch bend", "Pressure" }, dest[i]));
        l.add (std::make_unique<AudioParameterInt>    (id (PID::slot (i, "CC")), n + "CC", 0, 127, cc[i],
                                                       AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                       { return v == 1 ? String ("1 mod") : v == 11 ? String ("11 expr") : v == 74 ? String ("74 bright") : String (v); })));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Smooth")), n + "Smooth", NormalisableRange<float> (0.0f, 1000.0f, 0.0f, 0.4f), 60.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Lo")), n + "Min", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<AudioParameterFloat>  (id (PID::slot (i, "Hi")), n + "Max", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
        l.add (std::make_unique<AudioParameterBool>   (id (PID::slot (i, "Inv")), n + "Invert", false));
    }
    return l;
}

static juce::StringArray paramIds()
{
    juce::StringArray ids { PID::channel, PID::trigger, PID::note, PID::threshold, PID::speed, PID::velocity, PID::preview, PID::previewLevel };
    for (int i = 0; i < 4; ++i)
        for (auto* w : { "Src", "Dest", "CC", "Smooth", "Lo", "Hi", "Inv" }) ids.add (PID::slot (i, w));
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
    for (int s = 0; s < 4; ++s)
    {
        const int b = 8 + s * 7;
        p.slots[(size_t) s] = { i (b), i (b + 1), i (b + 2), f (b + 3), f (b + 4), f (b + 5), f (b + 6) > 0.5f };
    }
    return p;
}

void MouthProcessor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
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

    std::array<float, kNumSources> shape {};
    for (size_t i = 0; i < shape.size(); ++i) shape[i] = input.shape[i].load();
    const bool face = input.face.load();

    mapper.setParams (readParams());
    events.clear();
    mapper.process (shape, face, n / getSampleRate(), events);
    for (const auto& e : events)
    {
        switch (e.type)
        {
            case MidiOut::NoteOn:    midi.addEvent (juce::MidiMessage::noteOn (e.channel, e.data1, (juce::uint8) e.data2), 0); break;
            case MidiOut::NoteOff:   midi.addEvent (juce::MidiMessage::noteOff (e.channel, e.data1), 0); break;
            case MidiOut::CC:        midi.addEvent (juce::MidiMessage::controllerEvent (e.channel, e.data1, e.data2), 0); break;
            case MidiOut::PitchBend: midi.addEvent (juce::MidiMessage::pitchWheel (e.channel, e.data1), 0); break;
            case MidiOut::Pressure:  midi.addEvent (juce::MidiMessage::channelPressureChange (e.channel, e.data1), 0); break;
        }
    }
    for (size_t i = 0; i < slotOut.size(); ++i) slotOut[i] = mapper.slotOutput ((int) i);
    noteOn = mapper.noteSounding();

    if (raw[6]->load() > 0.5f && buffer.getNumChannels() > 0)
        voice.render (buffer.getWritePointer (0), buffer.getWritePointer (buffer.getNumChannels() > 1 ? 1 : 0), n,
                      mapper.getParams().triggerNote, mapper.gate(), shape, raw[7]->load());
}

void MouthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("DYMouth");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("mirror", mirror, nullptr);
    root.setProperty ("camera", cameraName, nullptr);
    const auto c = input.calibration();
    root.setProperty ("gapRest", c.gapRest, nullptr);
    root.setProperty ("widthRest", c.widthRest, nullptr);
    root.setProperty ("liftRest", c.liftRest, nullptr);
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
    Calibration c;
    c.gapRest = (float) (double) root.getProperty ("gapRest", c.gapRest);
    c.widthRest = (float) (double) root.getProperty ("widthRest", c.widthRest);
    c.liftRest = (float) (double) root.getProperty ("liftRest", c.liftRest);
    input.setCalibration (c);
}

juce::AudioProcessorEditor* MouthProcessor::createEditor() { return new MouthEditor (*this); }

} // namespace dy::mouth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dy::mouth::MouthProcessor(); }
