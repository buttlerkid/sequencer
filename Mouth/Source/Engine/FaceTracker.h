#pragma once
#include "MouthMapper.h"
#include <memory>
#include <string>
#include <vector>

// Faces and hands from a camera frame with MediaPipe's models (converted to ONNX,
// run by ONNX Runtime). Like MediaPipe: a detector finds each face or hand once,
// then the landmark model follows it from frame to frame on a crop around where it
// was; meanwhile the detector looks for new ones, a part of the frame at a time.

namespace dy::mouth {

struct Frame
{
    const unsigned char* rgb = nullptr;   // 3 bytes a pixel
    int width = 0, height = 0, stride = 0;
};

struct TrackResult
{
    int id = -1;                                 // its number, kept while it stays in view
    std::array<Point, Mesh::kCount> points {};   // frame pixels
    float score = 0;
};

struct HandResult
{
    int id = -1;
    std::array<Point, HandMesh::kCount> points {};
    float score = 0;
};

// The parts both trackers share: loading, finding new ones, following them.
class Tracker
{
public:
    Tracker();
    virtual ~Tracker();
    // runtimeDll: the full path of onnxruntime.dll; the models as bytes. False on
    // failure, with the reason in error().
    bool load (const std::wstring& runtimeDll, const void* detector, size_t detectorSize,
               const void* landmarks, size_t landmarksSize);
    bool isLoaded() const { return models != nullptr; }
    const std::string& error() const { return err; }
    void setMax (int n) { maxCount = n < 0 ? 0 : n; }
    void reset() { tracks.clear(); }

    struct Roi { float cx = 0, cy = 0, size = 0, angle = 0; };
    struct Models;

protected:
    struct Track { int id; Roi roi; };
    std::vector<Roi> detect (const Frame&, int window);

    // Follow every track, drop lost and doubled ones, then look for new ones.
    // landmarks (frame, roi, result&) -> bool; next (result) -> Roi
    template <typename Result, typename Landmarks, typename Next>
    std::vector<Result> follow (const Frame&, Landmarks&&, Next&&);

    // per kind: the detector's input, and its detection -> landmark crop
    int detSize = 128, lmSize = 256;
    float detMul = 2.0f / 255.0f, detAdd = -1.0f;
    std::vector<std::pair<int, int>> anchorLayers;     // (cells, anchors per cell)
    virtual Roi roiFromDetection (Point centre, float w, float h, const Point* keypoints) const = 0;

    std::unique_ptr<Models> models;
    std::string err;
    std::vector<Track> tracks;
    int maxCount = 4, frameNo = 0, tick = 0;
};

class FaceTracker : public Tracker
{
public:
    FaceTracker();
    std::vector<TrackResult> track (const Frame&);   // every face, up to the maximum
private:
    Roi roiFromDetection (Point centre, float w, float h, const Point* keypoints) const override;
    bool landmarks (const Frame&, const Roi&, TrackResult&);
};

class HandTracker : public Tracker
{
public:
    HandTracker();
    std::vector<HandResult> track (const Frame&);    // every hand, up to the maximum
private:
    Roi roiFromDetection (Point centre, float w, float h, const Point* keypoints) const override;
    bool landmarks (const Frame&, const Roi&, HandResult&);
};

} // namespace dy::mouth
