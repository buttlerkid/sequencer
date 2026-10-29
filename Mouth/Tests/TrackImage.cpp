// Run the face tracker on a still picture and print the mouth shape.
//   mouth_track_image <onnxruntime.dll> <models dir> <picture.ppm> [repeat]
#include "Engine/FaceTracker.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
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
    TrackResult r;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; ++i) r = t.track (f);
    const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / repeat;
    if (! r.face) { std::printf ("no face (%s)\n", t.error().c_str()); return 1; }
    const auto s = measure (r.points.data());
    const auto v = normalise (s, Calibration {});
    std::printf ("face %.2f  %.1f ms/frame\n", r.score, ms);
    std::printf ("eyes (%.0f,%.0f) (%.0f,%.0f)  lips %.0f,%.0f\n", r.points[Mesh::eyeR].x, r.points[Mesh::eyeR].y,
                 r.points[Mesh::eyeL].x, r.points[Mesh::eyeL].y, r.points[Mesh::innerTop].x, r.points[Mesh::innerTop].y);
    std::printf ("gap %.3f  width %.3f  lift %.3f   open %.2f width %.2f smile %.2f pucker %.2f\n",
                 s.gap, s.width, s.lift, v[Open], v[Width], v[Smile], v[Pucker]);
    return 0;
}
