// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/process/land_mask.h"
#include "vista/terrain/process/nv/thrust_gis.h"
#include "vista/terrain/process/bake_backend.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <tuple>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdlib>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  std::fprintf(stderr, "thrust_gis_cuda_built=%d\n",
               vista::thrust_gis_cuda_built() ? 1 : 0);
  uint8_t gpu_probe = 0;
  expect(!vista::try_fill_lonlat_mask_thrust(0.0, 0.0, 1.0, 1.0, 1, 1, nullptr,
                                            nullptr, nullptr, 0, 0, &gpu_probe),
         "Thrust GPU declines empty input (CPU fallback)");

  vista::LonLatRing box;
  box.x = {73.0, 135.0, 135.0, 73.0};
  box.y = {18.0, 18.0, 54.0, 54.0};
  expect(vista::point_in_lonlat_ring(116.0, 40.0, box), "beijing in box");
  expect(!vista::point_in_lonlat_ring(140.0, 35.0, box), "japan out");
  expect(!vista::point_in_lonlat_ring(116.0, 10.0, box),
         "south china sea out");

  vista::LonLatRing west;
  west.x = {70.0, 100.0, 100.0, 70.0};
  west.y = {20.0, 20.0, 50.0, 50.0};
  std::vector<vista::LonLatRing> rings = {west};
  expect(vista::any_ring_contains(88.0, 32.0, rings), "tibet in west");
  expect(!vista::any_ring_contains(121.0, 31.0, rings), "shanghai out");

  {
    std::vector<double> rx = box.x;
    std::vector<double> ry = box.y;
    int off[2] = {0, static_cast<int>(rx.size())};
    std::vector<uint8_t> gpu(static_cast<size_t>(8 * 8), 0);
    const bool gpu_ok = vista::try_fill_lonlat_mask_thrust(
        73.0, 18.0, 135.0, 54.0, 8, 8, rx.data(), ry.data(), off, 1,
        static_cast<int>(rx.size()), gpu.data());
    std::fprintf(stderr, "try_fill_lonlat_mask_thrust 8x8=%d\n", gpu_ok ? 1 : 0);
    if (vista::thrust_gis_cuda_built()) {
      expect(gpu_ok, "CUDA TU must run Thrust fill");
      expect(gpu[0] != 0, "box ring covers DEM cell 0");
    } else {
      expect(!gpu_ok, "no Toolkit: Thrust fill stays false");
    }
  }

  // china_city-scale: many high-vertex rings must not PIP every query.
  {
    std::vector<vista::LonLatRing> many;
    many.reserve(80);
    for (int i = 0; i < 80; ++i) {
      vista::LonLatRing ring;
      const double cx = 80.0 + static_cast<double>(i % 10) * 5.0;
      const double cy = 22.0 + static_cast<double>(i / 10) * 3.5;
      const int n = 120;
      ring.x.reserve(static_cast<size_t>(n));
      ring.y.reserve(static_cast<size_t>(n));
      for (int k = 0; k < n; ++k) {
        const double a = static_cast<double>(k) * 6.283185307179586 / n;
        ring.x.push_back(cx + 1.6 * std::cos(a));
        ring.y.push_back(cy + 1.2 * std::sin(a));
      }
      ring.prepare_bbox();
      many.push_back(std::move(ring));
    }
    const auto t0 = std::chrono::steady_clock::now();
    int hits = 0;
    for (int row = 0; row < 200; ++row) {
      const double lat = 54.0 - row * (54.0 - 17.5) / 199.0;
      for (int col = 0; col < 320; ++col) {
        const double lon = 73.0 + col * (135.0 - 73.0) / 319.0;
        if (vista::any_ring_contains(lon, lat, many)) {
          ++hits;
        }
      }
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    std::fprintf(stderr, "land_mask 80x120-gon x 320x200: %lld ms hits=%d\n",
                 static_cast<long long>(ms), hits);
    expect(hits > 0, "scattered rings produce land hits");
    // Serial point queries are the correctness oracle; wall time is not the
    // product path (fill_lonlat_mask below is).

    std::vector<uint8_t> mask(static_cast<size_t>(320 * 200), 0);
    const auto t1 = std::chrono::steady_clock::now();
    vista::fill_lonlat_mask(73.0, 17.5, 135.0, 54.0, 320, 200, many,
                            mask.data());
    const auto fill_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t1)
            .count();
    int fill_hits = 0;
    for (uint8_t v : mask) {
      fill_hits += v ? 1 : 0;
    }
    std::fprintf(stderr, "fill_lonlat_mask 320x200: %lld ms hits=%d\n",
                 static_cast<long long>(fill_ms), fill_hits);
    expect(fill_hits == hits, "fill_lonlat_mask matches point queries");
    expect(fill_ms < 500, "fill_lonlat_mask stays off UI-thread budget");
  }

  if (vista::bake_bench_wanted_from_env()) {
    std::vector<vista::LonLatRing> many;
    many.reserve(80);
    for (int i = 0; i < 80; ++i) {
      vista::LonLatRing ring;
      const double cx = 80.0 + static_cast<double>(i % 10) * 5.0;
      const double cy = 22.0 + static_cast<double>(i / 10) * 3.5;
      const int n = 120;
      ring.x.reserve(static_cast<size_t>(n));
      ring.y.reserve(static_cast<size_t>(n));
      for (int k = 0; k < n; ++k) {
        const double a = static_cast<double>(k) * 6.283185307179586 / n;
        ring.x.push_back(cx + 1.6 * std::cos(a));
        ring.y.push_back(cy + 1.2 * std::sin(a));
      }
      ring.prepare_bbox();
      many.push_back(std::move(ring));
    }
    auto run_fill = [&](const char* backend) {
      _putenv_s("BAKE_BACKEND", backend);
      std::vector<uint8_t> mask(static_cast<size_t>(320 * 200), 0);
      vista::reset_land_mask_bake_sample();
      const auto t0 = std::chrono::steady_clock::now();
      vista::fill_lonlat_mask(73.0, 17.5, 135.0, 54.0, 320, 200, many,
                              mask.data());
      const auto cold = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
      vista::LandMaskBakeSample s0 = vista::land_mask_last_bake_sample();
      mask.assign(mask.size(), 0);
      const auto t1 = std::chrono::steady_clock::now();
      vista::fill_lonlat_mask(73.0, 17.5, 135.0, 54.0, 320, 200, many,
                              mask.data());
      const auto warm = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t1)
                            .count();
      vista::LandMaskBakeSample s1 = vista::land_mask_last_bake_sample();
      std::fprintf(stderr,
                   "bake_bench land_mask backend=%s cuda=%d cold_ms=%lld "
                   "warm_ms=%lld sample_ms=%lld\n",
                   backend, s1.used_cuda, static_cast<long long>(cold),
                   static_cast<long long>(warm),
                   static_cast<long long>(s1.fill_ms));
      return std::tuple<int, long long, long long, int>(
          s0.used_cuda, cold, warm, s1.used_cuda);
    };
    const auto cpu = run_fill("cpu");
    expect(std::get<0>(cpu) == 0, "land_mask cpu not cuda");
    const auto gpu = run_fill("cuda");
    char exe_dir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
    if (char* slash = std::strrchr(exe_dir, '\\')) {
      slash[1] = '\0';
    }
    char p3[MAX_PATH] = {};
    std::snprintf(p3, sizeof(p3), "%scaptures", exe_dir);
    CreateDirectoryA(p3, nullptr);
    std::snprintf(p3, sizeof(p3), "%scaptures\\analysis", exe_dir);
    CreateDirectoryA(p3, nullptr);
    std::snprintf(p3, sizeof(p3), "%scaptures\\analysis\\hillshade_bake",
                  exe_dir);
    CreateDirectoryA(p3, nullptr);
    char outp[MAX_PATH] = {};
    std::snprintf(outp, sizeof(outp), "%s\\land_mask.json", p3);
    std::ofstream f(outp, std::ios::binary);
    if (f) {
      f << "{\n  \"grid\": \"320x200 rings=80 verts=120\",\n  \"rows\": [\n"
        << "    {\"backend\":\"cpu\",\"ok\":1,\"used_cuda\":" << std::get<0>(cpu)
        << ",\"cold_ms\":" << std::get<1>(cpu) << ",\"warm_ms\":"
        << std::get<2>(cpu) << "},\n"
        << "    {\"backend\":\"cuda\",\"ok\":" << std::get<3>(gpu)
        << ",\"used_cuda\":" << std::get<3>(gpu)
        << ",\"cold_ms\":" << std::get<1>(gpu) << ",\"warm_ms\":"
        << std::get<2>(gpu) << "}\n  ]\n}\n";
      std::fprintf(stderr, "hillshade_bake_bench wrote %s\n", outp);
    }
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d land_mask check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
