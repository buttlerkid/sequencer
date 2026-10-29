#pragma once
#include "MouthMapper.h"
#include <memory>
#include <string>
#include <vector>

// Face landmarks from a camera frame with MediaPipe's face models (converted to
// ONNX, run by ONNX Runtime). Like MediaPipe: a face detector finds each face once,
// then the landmark model follows it from frame to frame on a crop around where it
// was; meanwhile the detector looks for new faces, a part of the frame at a time.

namespace dy::mouth {

struct Frame
{
    const unsigned char* rgb = nullptr;   // 3 bytes a pixel
    int width = 0, height = 0, stride = 0;
};

struct TrackResult
{
    int id = -1;                                 // the face's number, 0..kMaxFaces-1, kept while it stays in view
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

    // Every face in the frame, up to maxFaces.
    std::vector<TrackResult> track (const Frame&);
    void setMaxFaces (int n) { maxFaces = n < 1 ? 1 : n > kMaxFaces ? kMaxFaces : n; }
    void reset() { tracks.clear(); }

private:
    struct Roi { float cx = 0, cy = 0, size = 0, angle = 0; };
    struct Track { int id; Roi roi; };
    std::vector<Roi> detect (const Frame&, int window);
    bool landmarks (const Frame&, const Roi&, TrackResult&);
    static Roi roiFrom (const TrackResult&);

    struct Impl;
    std::unique_ptr<Impl> impl;
    std::string err;
    std::vector<Track> tracks;
    int maxFaces = kMaxFaces, frameNo = 0;
};

} // namespace dy::mouth
