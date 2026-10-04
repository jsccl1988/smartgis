// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Standalone CPU convolve bench (no google/benchmark). GN owner: register as
// test/executable raster_convolve_bench; add raster_convolve_profile.cc to
// analysis_sources (symbols already live in raster_convolve.cc).
//
//   set SMT_ANALYSIS_PROFILE=1
//   out\Debug\raster_convolve_bench.exe
// Artifacts: out/<config>/log/raster_convolve_bench.json
//            out/<config>/captures/analysis/convolve/raster_convolve_bench.json

#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_convolve_profile.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct CaseResult {
  const char* kernel = "";
  int size = 0;
  const char* backend = "";
  double wall_ms = 0.0;
  double mpx_s = 0.0;
  double dispatch_ms = 0.0;
  double compute_ms = 0.0;
  bool ok = false;
};

std::vector<float> make_ramp(int n) {
  std::vector<float> v(static_cast<size_t>(n) * static_cast<size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      v[static_cast<size_t>(r) * static_cast<size_t>(n) + static_cast<size_t>(c)] =
          static_cast<float>(r) * 0.01f + static_cast<float>(c) * 0.001f;
    }
  }
  return v;
}

// Non-box 5x5 Gaussian-ish (odd, not uniform).
std::vector<double> gaussian5() {
  const double raw[25] = {
      1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4,
      6, 4, 1,
  };
  double sum = 0.0;
  for (double x : raw) {
    sum += x;
  }
  std::vector<double> k(25);
  for (int i = 0; i < 25; ++i) {
    k[static_cast<size_t>(i)] = raw[i] / sum;
  }
  return k;
}

double median3(double a, double b, double c) {
  if (a > b) {
    std::swap(a, b);
  }
  if (b > c) {
    std::swap(b, c);
  }
  if (a > b) {
    std::swap(a, b);
  }
  return b;
}

CaseResult run_case(const char* kernel_name, int n, int override_mode,
                    const std::vector<double>& kernel, int kw, int kh,
                    bool use_box3) {
  using Clock = std::chrono::steady_clock;
  CaseResult out;
  out.kernel = kernel_name;
  out.size = n;
  gis::detail::set_convolve_dispatch_override(override_mode);
  const std::vector<float> input = make_ramp(n);
  auto once = [&]() {
    if (use_box3) {
      return gis::detail::convolve_box3(input, n, n);
    }
    return gis::detail::convolve_raster(input, n, n, kernel, kw, kh);
  };
  (void)once();
  double walls[3] = {};
  gis::detail::RasterConvolveProfile last_snap;
  bool ok = false;
  for (int i = 0; i < 3; ++i) {
    const auto t0 = Clock::now();
    const auto r = once();
    const auto t1 = Clock::now();
    walls[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
    last_snap = gis::detail::last_convolve_profile();
    ok = r.ok;
  }
  gis::detail::set_convolve_dispatch_override(gis::detail::kConvolveDispatchAuto);
  out.wall_ms = median3(walls[0], walls[1], walls[2]);
  out.dispatch_ms = last_snap.dispatch_ms;
  out.compute_ms = last_snap.compute_ms;
  out.backend = last_snap.backend;
  out.ok = ok;
  const double pixels = static_cast<double>(n) * static_cast<double>(n);
  out.mpx_s = (out.wall_ms > 0.0) ? (pixels / (out.wall_ms * 1000.0)) : 0.0;
  return out;
}

std::filesystem::path artifact_root() {
  if (const char* e = std::getenv("SMARTGIS_OUT"); e && e[0]) {
    return std::filesystem::path(e);
  }
  const auto cwd = std::filesystem::current_path();
  const bool debug = std::filesystem::exists(cwd / "out" / "Debug");
  const bool release = std::filesystem::exists(cwd / "out" / "Release");
  if (debug || release) {
#ifdef NDEBUG
    if (release) {
      return cwd / "out" / "Release";
    }
#endif
    if (debug) {
      return cwd / "out" / "Debug";
    }
    return cwd / "out" / "Release";
  }
  return cwd;
}

void write_json(const std::filesystem::path& path,
                const std::vector<CaseResult>& rows) {
  std::filesystem::create_directories(path.parent_path());
  FILE* f = nullptr;
#if defined(_MSC_VER)
  fopen_s(&f, path.string().c_str(), "wb");
#else
  f = std::fopen(path.string().c_str(), "wb");
#endif
  if (!f) {
    std::fprintf(stderr, "WARN: cannot write %s\n", path.string().c_str());
    return;
  }
  std::fprintf(f, "{\n  \"min_pixels\": %d,\n  \"min_rows\": %d,\n  \"cases\": [\n",
               gis::detail::kParallelConvolveMinPixels,
               gis::detail::kParallelConvolveMinRows);
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto& r = rows[i];
    std::fprintf(f,
                 "    {\"kernel\":\"%s\",\"size\":%d,\"backend\":\"%s\","
                 "\"wall_ms\":%.4f,\"mpx_s\":%.4f,\"dispatch_ms\":%.4f,"
                 "\"compute_ms\":%.4f,\"ok\":%s}%s\n",
                 r.kernel, r.size, r.backend, r.wall_ms, r.mpx_s, r.dispatch_ms,
                 r.compute_ms, r.ok ? "true" : "false",
                 (i + 1 < rows.size()) ? "," : "");
  }
  std::fprintf(f, "  ]\n}\n");
  std::fclose(f);
  std::printf("wrote %s\n", path.string().c_str());
}

}  // namespace

int main() {
  const int sizes[] = {5, 64, 512, 1024};
  const std::vector<double> g5 = gaussian5();
  std::vector<CaseResult> rows;
  rows.reserve(24);

  std::printf("%-8s %6s %-8s %10s %10s %12s %12s\n", "kernel", "size",
              "backend", "wall_ms", "Mpx/s", "dispatch_ms", "compute_ms");

  for (int n : sizes) {
    const bool above = n >= gis::detail::kParallelConvolveMinRows &&
                       n * n >= gis::detail::kParallelConvolveMinPixels;
    const int modes[2] = {gis::detail::kConvolveDispatchSerial,
                          gis::detail::kConvolveDispatchParallel};
    const int nmodes = above ? 2 : 1;
    for (int mi = 0; mi < nmodes; ++mi) {
      const int mode = above ? modes[mi] : gis::detail::kConvolveDispatchAuto;
      {
        const CaseResult r =
            run_case("box3", n, mode, {}, 3, 3, /*use_box3=*/true);
        rows.push_back(r);
        std::printf("box3     %6d %-8s %10.4f %10.4f %12.4f %12.4f\n", r.size,
                    r.backend, r.wall_ms, r.mpx_s, r.dispatch_ms, r.compute_ms);
      }
      {
        const CaseResult r =
            run_case("gauss5", n, mode, g5, 5, 5, /*use_box3=*/false);
        rows.push_back(r);
        std::printf("gauss5   %6d %-8s %10.4f %10.4f %12.4f %12.4f\n", r.size,
                    r.backend, r.wall_ms, r.mpx_s, r.dispatch_ms, r.compute_ms);
      }
    }
  }

  const auto root = artifact_root();
  write_json(root / "log" / "raster_convolve_bench.json", rows);
  write_json(root / "captures" / "analysis" / "convolve" /
                 "raster_convolve_bench.json",
             rows);
  return 0;
}
