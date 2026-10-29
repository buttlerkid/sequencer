#pragma once
#include "MouthMapper.h"
#include <memory>
#include <string>

// Face landmarks from a camera frame with MediaPipe's face models (converted to
// ONNX, run by ONNX Runtime). Like MediaPipe: a face detector finds the face once,
// then the landmark model follows it from frame to frame on a crop around where it
// was, and the detector only runs again when the face is lost.

namespace dy::mouth {

struct Frame
{
    const unsigned char* rgb = nullptr;   // 3 bytes a pixel
    int width = 0, height = 0, stride = 0;
};

struct TrackResult
{
    bool face = false;
    std::array<Point, Mesh::kCount> points {};   // frame pixels
    float score = 0;
};

class FaceTracker
{
public:
    FaceTracker();
    ~FaceTracker();

    // runtimeDll: the full path of onnxruntime.dll; the models as bytes. False on
    // failure, with the reason in error().
    bool load (const std::wstring& runtimeDll, const void* detector, size_t detectorSize,
               const void* landmarks, size_t landmarksSize);
    bool isLoaded() const { return impl != nullptr; }
    const std::string& error() const { return err; }

    TrackResult track (const Frame&);
    void reset() { tracking = false; }

private:
    struct Roi { float cx = 0, cy = 0, size = 0, angle = 0; };
    bool detect (const Frame&, Roi&);
    bool landmarks (const Frame&, const Roi&, TrackResult&);

    struct Impl;
    std::unique_ptr<Impl> impl;
    std::string err;
    bool tracking = false;
    Roi roi;
};

} // namespace dy::mouth
