#include "FaceTracker.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// ONNX Runtime is loaded by hand from the plugin's own folder (a host may have
// another onnxruntime.dll loaded, and Windows has an old one in System32).
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>

namespace dy::mouth {

namespace
{
    float sigmoid (float x) { return 1.0f / (1.0f + std::exp (-std::clamp (x, -80.0f, 80.0f))); }

    // Sample a rotated square of the frame (centre, side, angle in frame pixels) into
    // an n x n RGB float image, value = byte * mul + add; outside the frame is black.
    void crop (const Frame& f, float cx, float cy, float size, float angle, int n, float mul, float add, float* dst)
    {
        const float c = std::cos (angle), s = std::sin (angle), step = size / (float) n;
        for (int v = 0; v < n; ++v)
            for (int u = 0; u < n; ++u)
            {
                const float du = (u + 0.5f) * step - size * 0.5f, dv = (v + 0.5f) * step - size * 0.5f;
                const float x = cx + c * du - s * dv - 0.5f, y = cy + s * du + c * dv - 0.5f;
                const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
                const float fx = x - (float) x0, fy = y - (float) y0;
                float rgb[3] = { 0, 0, 0 };
                for (int j = 0; j < 2; ++j)
                    for (int i = 0; i < 2; ++i)
                    {
                        const int xi = x0 + i, yi = y0 + j;
                        if (xi < 0 || yi < 0 || xi >= f.width || yi >= f.height) continue;
                        const float w = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
                        const unsigned char* p = f.rgb + (size_t) yi * (size_t) f.stride + (size_t) xi * 3;
                        rgb[0] += w * p[0]; rgb[1] += w * p[1]; rgb[2] += w * p[2];
                    }
                float* d = dst + ((size_t) v * (size_t) n + (size_t) u) * 3;
                d[0] = rgb[0] * mul + add; d[1] = rgb[1] * mul + add; d[2] = rgb[2] * mul + add;
            }
    }

    // The dll and the one ORT environment, shared by the face and hand trackers.
    struct Runtime
    {
        HMODULE dll = nullptr;
        std::unique_ptr<Ort::Env> env;
        ~Runtime() { env.reset(); }   // the dll stays loaded: ORT keeps threads and statics that do not like being unloaded
    };

    std::shared_ptr<Runtime> sharedRuntime (const std::wstring& path, std::string& err)
    {
        static std::mutex lock;
        static std::weak_ptr<Runtime> shared;
        std::lock_guard<std::mutex> g (lock);
        if (auto rt = shared.lock()) return rt;
        auto rt = std::make_shared<Runtime>();
        rt->dll = LoadLibraryExW (path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (rt->dll == nullptr) { err = "ONNX Runtime not found next to the plugin"; return {}; }
        using GetApiBase = const OrtApiBase* (ORT_API_CALL*) ();
        auto getBase = reinterpret_cast<GetApiBase> (GetProcAddress (rt->dll, "OrtGetApiBase"));
        const OrtApi* api = getBase != nullptr ? getBase()->GetApi (ORT_API_VERSION) : nullptr;
        if (api == nullptr) { err = "ONNX Runtime is the wrong version"; return {}; }
        Ort::InitApi (api);
        try { rt->env = std::make_unique<Ort::Env> (ORT_LOGGING_LEVEL_ERROR, "DYMouth"); }
        catch (const std::exception& e) { err = e.what(); return {}; }
        shared = rt;
        return rt;
    }

    struct Model
    {
        std::unique_ptr<Ort::Session> session;
        std::vector<std::string> inNames, outNames;
        std::vector<const char*> in, out;

        void open (Ort::Env& env, const void* data, size_t size)
        {
            Ort::SessionOptions so;
            so.SetIntraOpNumThreads (2);              // leave the rest of the machine to the audio
            so.SetInterOpNumThreads (1);
            // waiting threads sleep instead of spinning: otherwise they burn whole cores between frames
            so.AddConfigEntry ("session.intra_op.allow_spinning", "0");
            so.AddConfigEntry ("session.inter_op.allow_spinning", "0");
            so.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
            session = std::make_unique<Ort::Session> (env, data, size, so);
            Ort::AllocatorWithDefaultOptions alloc;
            for (size_t i = 0; i < session->GetInputCount(); ++i)  inNames.push_back (session->GetInputNameAllocated (i, alloc).get());
            for (size_t i = 0; i < session->GetOutputCount(); ++i) outNames.push_back (session->GetOutputNameAllocated (i, alloc).get());
            for (auto& s : inNames) in.push_back (s.c_str());
            for (auto& s : outNames) out.push_back (s.c_str());
        }

        std::vector<Ort::Value> run (std::vector<float>& input, int n)
        {
            const std::array<int64_t, 4> shape { 1, n, n, 3 };
            auto mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
            auto tensor = Ort::Value::CreateTensor<float> (mem, input.data(), input.size(), shape.data(), shape.size());
            return session->Run (Ort::RunOptions { nullptr }, in.data(), &tensor, 1, out.data(), out.size());
        }
    };

    bool closeTo (const Tracker::Roi& a, const Tracker::Roi& b, float k) { return std::hypot (a.cx - b.cx, a.cy - b.cy) < k; }

    // The landmark crop from points: around their bounding box along the given angle,
    // scaled, and moved `shiftUp` of its side towards the crop's top.
    Tracker::Roi roiAround (const Point* pts, int n, float angle, float scale, float shiftUp)
    {
        const float c = std::cos (angle), s = std::sin (angle);
        float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
        for (int i = 0; i < n; ++i)
        {
            const float u = c * pts[i].x + s * pts[i].y, v = -s * pts[i].x + c * pts[i].y;
            x0 = std::min (x0, u); x1 = std::max (x1, u); y0 = std::min (y0, v); y1 = std::max (y1, v);
        }
        const float side = std::max (x1 - x0, y1 - y0);
        const float u = 0.5f * (x0 + x1), v = 0.5f * (y0 + y1) - shiftUp * side;
        return { c * u - s * v, s * u + c * v, side * scale, angle };
    }

    // A landmark model's points (x, y, z each, in crop pixels) back in the frame.
    void toFrame (const float* xyz, int n, const Tracker::Roi& r, int size, Point* out)
    {
        const float c = std::cos (r.angle), s = std::sin (r.angle), k = r.size / (float) size, h = size * 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float du = (xyz[i * 3] - h) * k, dv = (xyz[i * 3 + 1] - h) * k;
            out[i] = { r.cx + c * du - s * dv, r.cy + s * du + c * dv };
        }
    }

    // the angle that turns a hand's crop so the fingers (wrist -> middle knuckle) point up
    float handAngle (Point wrist, Point middle) { return std::atan2 (middle.x - wrist.x, wrist.y - middle.y); }
}

struct Tracker::Models
{
    std::shared_ptr<Runtime> runtime;
    Model detector, landmarks;
    std::vector<Point> anchors;
    std::vector<float> detIn, lmIn;
    ~Models() { detector.session.reset(); landmarks.session.reset(); }
};

Tracker::Tracker() = default;
Tracker::~Tracker() = default;

bool Tracker::load (const std::wstring& runtimeDll, const void* det, size_t detBytes, const void* lm, size_t lmBytes)
{
    models.reset();
    auto m = std::make_unique<Models>();
    m->runtime = sharedRuntime (runtimeDll, err);
    if (m->runtime == nullptr) return false;
    try
    {
        m->detector.open (*m->runtime->env, det, detBytes);
        m->landmarks.open (*m->runtime->env, lm, lmBytes);
    }
    catch (const std::exception& e) { err = std::string ("Model: ") + e.what(); return false; }
    // SSD anchors: the centre of every cell, several per cell
    for (auto [cells, perCell] : anchorLayers)
        for (int y = 0; y < cells; ++y)
            for (int x = 0; x < cells; ++x)
                for (int k = 0; k < perCell; ++k)
                    m->anchors.push_back ({ (x + 0.5f) / (float) cells, (y + 0.5f) / (float) cells });
    m->detIn.resize ((size_t) detSize * (size_t) detSize * 3);
    m->lmIn.resize ((size_t) lmSize * (size_t) lmSize * 3);
    models = std::move (m);
    err.clear();
    return true;
}

std::vector<Tracker::Roi> Tracker::detect (const Frame& f, int window)
{
    // The detector sees a small square, so faces and hands further back get tiny in
    // the whole frame: window 0 is the whole frame, the others overlapping zoomed-in
    // tiles, one a frame in turn (all at once would stall the video).
    struct Hit { float score, x0, y0, x1, y1; Roi roi; };
    std::vector<Hit> hits;
    const float ds = (float) detSize;
    auto look = [&] (float cx, float cy, float side)
    {
        crop (f, cx, cy, side, 0.0f, detSize, detMul, detAdd, models->detIn.data());
        auto res = models->detector.run (models->detIn, detSize);
        const int stride = (int) res[0].GetTensorTypeAndShapeInfo().GetShape().back();   // 4 for the box + 2 a keypoint
        const float* reg = res[0].GetTensorData<float>();
        const float* cls = res[1].GetTensorData<float>();
        auto toFrameXY = [&] (float nx, float ny) { return Point { cx - side * 0.5f + nx * side, cy - side * 0.5f + ny * side }; };
        for (size_t i = 0; i < models->anchors.size(); ++i)
            if (const float s = sigmoid (cls[i]); s > 0.5f)
            {
                const float* r = reg + i * (size_t) stride;
                const Point a = models->anchors[i];
                const Point c = toFrameXY (a.x + r[0] / ds, a.y + r[1] / ds);
                const float w = r[2] / ds * side, h = r[3] / ds * side;
                Point kp[8] {};
                for (int k = 0; k < std::min (8, (stride - 4) / 2); ++k)
                    kp[k] = toFrameXY (a.x + r[4 + 2 * k] / ds, a.y + r[5 + 2 * k] / ds);
                hits.push_back ({ s, c.x - w * 0.5f, c.y - h * 0.5f, c.x + w * 0.5f, c.y + h * 0.5f, roiFromDetection (c, w, h, kp) });
            }
    };
    const float W = (float) f.width, H = (float) f.height;
    const float tile = 0.6f * std::min (W, H);
    const int cols = (int) std::ceil ((W - tile) / (tile * 0.6f)) + 1, rows = (int) std::ceil ((H - tile) / (tile * 0.6f)) + 1;
    window %= 1 + cols * rows;
    if (window == 0) look (W * 0.5f, H * 0.5f, std::max (W, H));
    else
    {
        const int c = (window - 1) % cols, r = (window - 1) / cols;
        look (tile * 0.5f + (cols > 1 ? (W - tile) * (float) c / (float) (cols - 1) : (W - tile) * 0.5f),
              tile * 0.5f + (rows > 1 ? (H - tile) * (float) r / (float) (rows - 1) : (H - tile) * 0.5f), tile);
    }
    std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.score > b.score; });

    // keep the best of each overlapping group (non-maximum suppression)
    auto iou = [] (const Hit& a, const Hit& b)
    {
        const float w = std::max (0.0f, std::min (a.x1, b.x1) - std::max (a.x0, b.x0));
        const float h = std::max (0.0f, std::min (a.y1, b.y1) - std::max (a.y0, b.y0));
        const float inter = w * h, uni = (a.x1 - a.x0) * (a.y1 - a.y0) + (b.x1 - b.x0) * (b.y1 - b.y0) - inter;
        return uni > 0.0f ? inter / uni : 0.0f;
    };
    std::vector<Hit> kept;
    for (const auto& h : hits)
        if (! std::any_of (kept.begin(), kept.end(), [&] (const Hit& k) { return iou (h, k) > 0.3f; }))
            kept.push_back (h);
    std::vector<Roi> out;
    for (const auto& h : kept)
        if (h.roi.size > 8.0f && out.size() < 4) out.push_back (h.roi);
    return out;
}

template <typename Result, typename Landmarks, typename Next>
std::vector<Result> Tracker::follow (const Frame& f, Landmarks&& landmarks, Next&& next)
{
    std::vector<Result> results;
    if (models == nullptr || f.rgb == nullptr || f.width < 16 || f.height < 16 || maxCount == 0) { tracks.clear(); return results; }
    try
    {
        // follow the ones we have, each on a crop around where it was
        std::vector<Track> kept;
        for (const auto& t : tracks)
        {
            Result r;
            if (! landmarks (f, t.roi, r)) continue;
            const Roi n = next (r);
            // two crops that slid onto the same one: keep the first
            if (std::any_of (kept.begin(), kept.end(), [&] (const Track& k) { return closeTo (k.roi, n, 0.3f * std::min (k.roi.size, n.size)); }))
                continue;
            r.id = t.id;
            kept.push_back ({ t.id, n });
            results.push_back (r);
        }
        tracks = std::move (kept);
        while ((int) tracks.size() > maxCount) { tracks.pop_back(); results.pop_back(); }

        // look for new ones, a window at a time: every frame while there are none, every third otherwise
        if ((int) tracks.size() < maxCount && (tracks.empty() || ++tick % 3 == 0))
            for (const auto& roi : detect (f, frameNo++))
            {
                if ((int) tracks.size() >= maxCount) break;
                if (std::any_of (tracks.begin(), tracks.end(), [&] (const Track& t) { return closeTo (t.roi, roi, 0.5f * std::max (t.roi.size, roi.size)); }))
                    continue;
                Result r;
                if (! landmarks (f, roi, r)) continue;
                int id = 0;                       // the lowest free number
                while (std::any_of (tracks.begin(), tracks.end(), [&] (const Track& t) { return t.id == id; })) ++id;
                r.id = id;
                tracks.push_back ({ id, next (r) });
                results.push_back (r);
            }
        err.clear();
    }
    catch (const std::exception& e) { err = e.what(); tracks.clear(); results.clear(); }
    return results;
}

// ------------------------------------------------------------------ faces
FaceTracker::FaceTracker()
{
    // BlazeFace short range: 128 px, -1..1, 16x16 cells x 2 at stride 8, 8x8 x 6 at stride 16
    detSize = 128; lmSize = 256; detMul = 2.0f / 255.0f; detAdd = -1.0f;
    anchorLayers = { { 16, 2 }, { 8, 6 } };
    maxCount = kMaxFaces;
}

Tracker::Roi FaceTracker::roiFromDetection (Point c, float w, float h, const Point* kp) const
{
    // kp 0 the person's right eye (left in the image), kp 1 the left: 1.5 x the box, turned with the eyes
    return { c.x, c.y, std::max (w, h) * 1.5f, std::atan2 (kp[1].y - kp[0].y, kp[1].x - kp[0].x) };
}

bool FaceTracker::landmarks (const Frame& f, const Roi& r, TrackResult& res)
{
    crop (f, r.cx, r.cy, r.size, r.angle, lmSize, 1.0f / 255.0f, 0.0f, models->lmIn.data());
    auto out = models->landmarks.run (models->lmIn, lmSize);
    res.score = sigmoid (out[1].GetTensorData<float>()[0]);
    if (res.score < 0.5f) return false;
    toFrame (out[0].GetTensorData<float>(), Mesh::kCount, r, lmSize, res.points.data());
    return true;
}

std::vector<TrackResult> FaceTracker::track (const Frame& f)
{
    return follow<TrackResult> (f, [this] (const Frame& fr, const Roi& r, TrackResult& res) { return landmarks (fr, r, res); },
                                [] (const TrackResult& res)
                                {
                                    const Point e0 = res.points[Mesh::eyeR], e1 = res.points[Mesh::eyeL];
                                    return roiAround (res.points.data(), Mesh::kCount, std::atan2 (e1.y - e0.y, e1.x - e0.x), 1.5f, 0.0f);
                                });
}

// ------------------------------------------------------------------ hands
HandTracker::HandTracker()
{
    // palm detection: 192 px, 0..1, 24x24 cells x 2 at stride 8, 12x12 x 6 at stride 16
    detSize = 192; lmSize = 224; detMul = 1.0f / 255.0f; detAdd = 0.0f;
    anchorLayers = { { 24, 2 }, { 12, 6 } };
    maxCount = kMaxHands;
}

Tracker::Roi HandTracker::roiFromDetection (Point c, float w, float h, const Point* kp) const
{
    // kp 0 the wrist, kp 2 the middle finger's knuckle: 2.6 x the palm, half a palm towards the fingers
    const float a = handAngle (kp[0], kp[2]), m = std::max (w, h);
    return { c.x + std::sin (a) * 0.5f * m, c.y - std::cos (a) * 0.5f * m, m * 2.6f, a };
}

bool HandTracker::landmarks (const Frame& f, const Roi& r, HandResult& res)
{
    crop (f, r.cx, r.cy, r.size, r.angle, lmSize, 1.0f / 255.0f, 0.0f, models->lmIn.data());
    auto out = models->landmarks.run (models->lmIn, lmSize);
    res.score = out[1].GetTensorData<float>()[0];       // already a probability
    if (res.score < 0.5f) return false;
    toFrame (out[0].GetTensorData<float>(), HandMesh::kCount, r, lmSize, res.points.data());
    return true;
}

std::vector<HandResult> HandTracker::track (const Frame& f)
{
    return follow<HandResult> (f, [this] (const Frame& fr, const Roi& r, HandResult& res) { return landmarks (fr, r, res); },
                               [] (const HandResult& res)
                               {
                                   // like MediaPipe: around the palm and the fingers' lower joints (not the tips, which wander)
                                   static constexpr int palm[] { 0, 1, 2, 3, 5, 6, 9, 10, 13, 14, 17, 18 };
                                   Point pts[std::size (palm)];
                                   for (size_t i = 0; i < std::size (palm); ++i) pts[i] = res.points[(size_t) palm[i]];
                                   const float a = handAngle (res.points[HandMesh::wrist], res.points[HandMesh::middleKnuckle]);
                                   return roiAround (pts, (int) std::size (palm), a, 2.0f, 0.1f);
                               });
}

} // namespace dy::mouth
