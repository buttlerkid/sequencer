// Run the face tracker on a still picture and print the mouth shape.
//   mouth_track_image <onnxruntime.dll> <models dir> <picture.ppm> [repeat]
#include "Engine/FaceTracker.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <sstream>
#include <vector>

using namespace dy::mouth;

static std::vector<char> readAll (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    return { std::istreambuf_iterator<char> (f), {} };
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::puts ("usage: mouth_track_image <onnxruntime.dll> <models dir> <picture.ppm> [repeat]"); return 2; }
    const std::string dll = argv[1], dir = argv[2];
    auto det = readAll (dir + "/face_detector.onnx"), lm = readAll (dir + "/face_landmarks_detector.onnx");
    FaceTracker t;
    if (! t.load (std::wstring (dll.begin(), dll.end()), det.data(), det.size(), lm.data(), lm.size()))
    {
        std::printf ("load failed: %s\n", t.error().c_str());
        return 1;
    }

    // binary PPM (P6, 8 bit)
    auto ppm = readAll (argv[3]);
    std::istringstream head (std::string (ppm.begin(), ppm.begin() + std::min<size_t> (ppm.size(), 64)));
    std::string magic; int w = 0, h = 0, maxv = 0;
    head >> magic >> w >> h >> maxv;
    const size_t offset = (size_t) head.tellg() + 1;
    if (magic != "P6" || w <= 0 || h <= 0 || ppm.size() < offset + (size_t) w * h * 3) { std::puts ("not a P6 ppm"); return 1; }
    Frame f { reinterpret_cast<const unsigned char*> (ppm.data() + offset), w, h, w * 3 };

    const int repeat = argc > 4 ? std::atoi (argv[4]) : 1;
    std::vector<TrackResult> faces;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; ++i) faces = t.track (f);
    const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / repeat;
    std::printf ("%d face(s)  %.1f ms/frame  %s\n", (int) faces.size(), ms, t.error().c_str());
    for (const auto& r : faces)
    {
        const auto s = measure (r.points.data(), (float) w, (float) h);
        const auto v = normalise (s, Calibration {});
        std::printf ("face %d (%.2f)  eyes (%.0f,%.0f) (%.0f,%.0f)\n", r.id + 1, r.score, r.points[Mesh::eyeR].x, r.points[Mesh::eyeR].y,
                     r.points[Mesh::eyeL].x, r.points[Mesh::eyeL].y);
        std::printf ("  gap %.3f width %.3f lift %.3f brow %.3f roll %.2f head %.2f,%.2f\n", s.gap, s.width, s.lift, s.brow, s.roll, s.headX, s.headY);
        std::printf ("  open %.2f width %.2f smile %.2f pucker %.2f brows %.2f tilt %.2f\n", v[Open], v[Width], v[Smile], v[Pucker], v[Brows], v[Tilt]);
    }

    auto hdet = readAll (dir + "/hand_detector.onnx"), hlm = readAll (dir + "/hand_landmarks_detector.onnx");
    HandTracker ht;
    if (! ht.load (std::wstring (dll.begin(), dll.end()), hdet.data(), hdet.size(), hlm.data(), hlm.size()))
    {
        std::printf ("hand load failed: %s\n", ht.error().c_str());
        return 1;
    }
    std::vector<HandResult> hands;
    const auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; ++i) hands = ht.track (f);
    const double hms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t1).count() / repeat;
    std::printf ("%d hand(s)  %.1f ms/frame  %s\n", (int) hands.size(), hms, ht.error().c_str());
    for (const auto& r : hands)
    {
        const auto s = measureHand (r.points.data(), (float) w, (float) h);
        std::printf ("hand %d (%.2f)  wrist (%.0f,%.0f) index tip (%.0f,%.0f)  pinch %.2f spread %.2f at %.2f,%.2f\n", r.id + 1, r.score,
                     r.points[HandMesh::wrist].x, r.points[HandMesh::wrist].y, r.points[HandMesh::indexTip].x, r.points[HandMesh::indexTip].y,
                     s.pinch, s.spread, s.x, s.y);
    }
    return faces.empty() && hands.empty() ? 1 : 0;
}
