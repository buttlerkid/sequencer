#include "FaceTracker.h"
#include <algorithm>
#include <cmath>
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
    constexpr int kDetSize = 128, kLmSize = 256, kAnchors = 896;
    constexpr float kRoiScale = 1.5f;      // MediaPipe's margin around the face for the landmark crop

    float sigmoid (float x) { return 1.0f / (1.0f + std::exp (-std::clamp (x, -80.0f, 80.0f))); }

    // BlazeFace short range anchors: 16x16 cells x 2 at stride 8, 8x8 cells x 6 at stride 16.
    std::vector<Point> makeAnchors()
    {
        std::vector<Point> a;
        a.reserve (kAnchors);
        for (auto [cells, perCell] : { std::pair { 16, 2 }, std::pair { 8, 6 } })
            for (int y = 0; y < cells; ++y)
                for (int x = 0; x < cells; ++x)
                    for (int k = 0; k < perCell; ++k)
                        a.push_back ({ (x + 0.5f) / (float) cells, (y + 0.5f) / (float) cells });
        return a;
    }

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
}

struct FaceTracker::Impl
{
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

    HMODULE dll = nullptr;
    std::unique_ptr<Ort::Env> env;
    Model detector, mesh;
    std::vector<Point> anchors = makeAnchors();
    std::vector<float> detIn = std::vector<float> ((size_t) kDetSize * kDetSize * 3),
                       lmIn  = std::vector<float> ((size_t) kLmSize * kLmSize * 3);

    ~Impl()
    {
        detector.session.reset();
        mesh.session.reset();
        env.reset();
        // the dll stays loaded: ORT keeps threads and statics that do not like being unloaded
    }
};

FaceTracker::FaceTracker() = default;
FaceTracker::~FaceTracker() = default;

bool FaceTracker::load (const std::wstring& runtimeDll, const void* det, size_t detSize, const void* lm, size_t lmSize)
{
    impl.reset();
    auto m = std::make_unique<Impl>();
    m->dll = LoadLibraryExW (runtimeDll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (m->dll == nullptr) { err = "ONNX Runtime not found next to the plugin"; return false; }
    using GetApiBase = const OrtApiBase* (ORT_API_CALL*) ();
    auto getBase = reinterpret_cast<GetApiBase> (GetProcAddress (m->dll, "OrtGetApiBase"));
    const OrtApi* api = getBase != nullptr ? getBase()->GetApi (ORT_API_VERSION) : nullptr;
    if (api == nullptr) { err = "ONNX Runtime is the wrong version"; return false; }
    Ort::InitApi (api);
    try
    {
        m->env = std::make_unique<Ort::Env> (ORT_LOGGING_LEVEL_ERROR, "DYMouth");
        m->detector.open (*m->env, det, detSize);
        m->mesh.open (*m->env, lm, lmSize);
    }
    catch (const std::exception& e) { err = std::string ("Face model: ") + e.what(); return false; }
    impl = std::move (m);
    err.clear();
    return true;
}

bool FaceTracker::landmarks (const Frame& f, const Roi& r, TrackResult& res)
{
    crop (f, r.cx, r.cy, r.size, r.angle, kLmSize, 1.0f / 255.0f, 0.0f, impl->lmIn.data());
    auto out = impl->mesh.run (impl->lmIn, kLmSize);
    const float* pts = out[0].GetTensorData<float>();
    res.score = sigmoid (out[1].GetTensorData<float>()[0]);
    if (res.score < 0.5f) return false;

    const float c = std::cos (r.angle), s = std::sin (r.angle), k = r.size / kLmSize;
    for (int i = 0; i < Mesh::kCount; ++i)
    {
        const float du = (pts[i * 3] - kLmSize * 0.5f) * k, dv = (pts[i * 3 + 1] - kLmSize * 0.5f) * k;
        res.points[(size_t) i] = { r.cx + c * du - s * dv, r.cy + s * du + c * dv };
    }
    return true;
}

std::vector<FaceTracker::Roi> FaceTracker::detect (const Frame& f, int window)
{
    // The detector sees a 128 px square, so faces across the room get tiny in the
    // whole frame: window 0 is the whole frame, the others overlapping zoomed-in
    // tiles, one a frame in turn (all at once would stall the video).
    struct Hit { float score, x0, y0, x1, y1; Roi roi; };
    std::vector<Hit> hits;
    auto look = [&] (float cx, float cy, float side)
    {
        crop (f, cx, cy, side, 0.0f, kDetSize, 2.0f / 255.0f, -1.0f, impl->detIn.data());
        auto res = impl->detector.run (impl->detIn, kDetSize);
        const float* reg = res[0].GetTensorData<float>();
        const float* cls = res[1].GetTensorData<float>();
        auto toFrame = [&] (float nx, float ny) { return Point { cx - side * 0.5f + nx * side, cy - side * 0.5f + ny * side }; };
        for (int i = 0; i < kAnchors; ++i)
            if (const float s = sigmoid (cls[i]); s > 0.5f)
            {
                const float* r = reg + (size_t) i * 16;
                const Point a = impl->anchors[(size_t) i];
                const Point c = toFrame (a.x + r[0] / kDetSize, a.y + r[1] / kDetSize);
                const float w = r[2] / kDetSize * side, h = r[3] / kDetSize * side;
                const Point eyeR = toFrame (a.x + r[4] / kDetSize, a.y + r[5] / kDetSize);   // the person's right eye, left in the image
                const Point eyeL = toFrame (a.x + r[6] / kDetSize, a.y + r[7] / kDetSize);
                hits.push_back ({ s, c.x - w * 0.5f, c.y - h * 0.5f, c.x + w * 0.5f, c.y + h * 0.5f,
                                  { c.x, c.y, std::max (w, h) * kRoiScale, std::atan2 (eyeL.y - eyeR.y, eyeL.x - eyeR.x) } });
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
    {
        if (std::any_of (kept.begin(), kept.end(), [&] (const Hit& k) { return iou (h, k) > 0.3f; })) continue;
        kept.push_back (h);
    }
    std::vector<Roi> out;
    for (const auto& h : kept)
        if (h.roi.size > 8.0f && (int) out.size() < kMaxFaces) out.push_back (h.roi);
    return out;
}

FaceTracker::Roi FaceTracker::roiFrom (const TrackResult& res)
{
    // around the landmarks, turned with the eyes
    const Point e0 = res.points[Mesh::eyeR], e1 = res.points[Mesh::eyeL];
    const float angle = std::atan2 (e1.y - e0.y, e1.x - e0.x), c = std::cos (angle), s = std::sin (angle);
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (const auto& p : res.points)
    {
        const float u = c * p.x + s * p.y, v = -s * p.x + c * p.y;
        x0 = std::min (x0, u); x1 = std::max (x1, u); y0 = std::min (y0, v); y1 = std::max (y1, v);
    }
    const float u = 0.5f * (x0 + x1), v = 0.5f * (y0 + y1);
    return { c * u - s * v, s * u + c * v, std::max (x1 - x0, y1 - y0) * kRoiScale, angle };
}

std::vector<TrackResult> FaceTracker::track (const Frame& f)
{
    std::vector<TrackResult> results;
    if (impl == nullptr || f.rgb == nullptr || f.width < 16 || f.height < 16) return results;
    try
    {
        // follow the faces we have, each on a crop around where it was
        std::vector<Track> kept;
        for (const auto& t : tracks)
        {
            TrackResult r;
            if (! landmarks (f, t.roi, r)) continue;
            const Roi next = roiFrom (r);
            // two crops that slid onto the same face: keep the first
            const bool twin = std::any_of (kept.begin(), kept.end(), [&] (const Track& k)
                              { return std::hypot (k.roi.cx - next.cx, k.roi.cy - next.cy) < 0.3f * std::min (k.roi.size, next.size); });
            if (twin) continue;
            r.id = t.id;
            kept.push_back ({ t.id, next });
            results.push_back (r);
        }
        tracks = std::move (kept);
        while ((int) tracks.size() > maxFaces) { tracks.pop_back(); results.pop_back(); }

        // look for new faces, one window a frame
        if ((int) tracks.size() < maxFaces)
            for (const auto& roi : detect (f, frameNo++))
            {
                if ((int) tracks.size() >= maxFaces) break;
                const bool known = std::any_of (tracks.begin(), tracks.end(), [&] (const Track& t)
                                   { return std::hypot (t.roi.cx - roi.cx, t.roi.cy - roi.cy) < 0.5f * std::max (t.roi.size, roi.size); });
                if (known) continue;
                TrackResult r;
                if (! landmarks (f, roi, r)) continue;
                int id = 0;                       // the lowest free number
                while (std::any_of (tracks.begin(), tracks.end(), [&] (const Track& t) { return t.id == id; })) ++id;
                r.id = id;
                tracks.push_back ({ id, roiFrom (r) });
                results.push_back (r);
            }
        err.clear();
    }
    catch (const std::exception& e) { err = e.what(); tracks.clear(); results.clear(); }
    return results;
}

} // namespace dy::mouth
