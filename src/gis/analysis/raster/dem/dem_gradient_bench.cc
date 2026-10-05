// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// CPU-only DEM gradient bench (serial vs parallel_for).
//
//   set ANALYSIS_PROFILE=1
//   out\Debug\analysis_dem_gradient_bench.exe
// Artifacts: out/<config>/log/dem_gradient_bench.json
//            out/<config>/captures/analysis/dem_gradient/dem_gradient_bench.json

#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/dem/dem_gradient_profile.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct CaseResult {
  int size = 0;
  const char* backend = "";
  double wall_ms = 0.0;
  double mpx_s = 0.0;
  double dispatch_ms = 0.0;
  double compute_ms = 0.0;
  int threads = 0;
  bool ok = false;
};

std::vector<float> make_ramp(int n) {
  std::vector<float> v(static_cast<size_t>(n) * static_cast<size_t>(n));
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      v[static_cast<size_t>(r) * static_cast<size_t>(n) +
        static_cast<size_t>(c)] = static_cast<float>(c);
    }
  }
  return v;
}

double median_of(std::vector<double> samples) {
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const size_t n = samples.size();
  if (n % 2u == 1u) {
    return samples[n / 2u];
  }
  return 0.5 * (samples[n / 2u - 1u] + samples[n / 2u]);
}

CaseResult run_case(int n, int override_mode) {
  using Clock = std::chrono::steady_clock;
  CaseResult out;
  out.size = n;
  gis::detail::set_dem_gradient_dispatch_override(override_mode);
  const std::vector<float> elev = make_ramp(n);
  auto once = [&]() {
    return gis::detail::compute_dem_gradient(elev, n, n, 1.0, 1.0);
  };
  (void)once();
  constexpr int kReps = 7;
  std::vector<double> walls;
  walls.reserve(static_cast<size_t>(kReps));
  gis::detail::DemGradientProfile last_snap;
  bool ok = false;
  for (int i = 0; i < kReps; ++i) {
    const auto t0 = Clock::now();
    const auto r = once();
    const auto t1 = Clock::now();
    walls.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    last_snap = gis::detail::last_dem_gradient_profile();
    ok = r.ok;
  }
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchAuto);
  out.wall_ms = median_of(std::move(walls));
  out.dispatch_ms = last_snap.dispatch_ms;
  out.compute_ms = last_snap.compute_ms;
  out.backend = last_snap.backend;
  out.threads = last_snap.threads;
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
  std::fprintf(f,
               "{\n  \"min_pixels\": %d,\n  \"min_rows\": %d,\n  \"cases\": [\n",
               gis::detail::kParallelGradientMinPixels,
               gis::detail::kParallelGradientMinRows);
  for (size_t i = 0; i < rows.size(); ++i) {
    const auto& r = rows[i];
    std::fprintf(f,
                 "    {\"size\":%d,\"backend\":\"%s\",\"threads\":%d,"
                 "\"wall_ms\":%.4f,\"mpx_s\":%.4f,\"dispatch_ms\":%.4f,"
                 "\"compute_ms\":%.4f,\"ok\":%s}%s\n",
                 r.size, r.backend, r.threads, r.wall_ms, r.mpx_s,
                 r.dispatch_ms, r.compute_ms, r.ok ? "true" : "false",
                 (i + 1 < rows.size()) ? "," : "");
  }
  std::fprintf(f, "  ]\n}\n");
  std::fclose(f);
  std::printf("wrote %s\n", path.string().c_str());
}

}  // namespace

int main() {
  const int sizes[] = {4, 64, 128, 512, 1024};
  std::vector<CaseResult> rows;
  rows.reserve(16);

  std::printf("threshold pixels>=%d rows>=%d pool_threads=%d (CPU only)\n",
              gis::detail::kParallelGradientMinPixels,
              gis::detail::kParallelGradientMinRows,
              gis::detail::kDemGradientPoolThreads);
  std::printf("median of 7 timed reps after 1 warmup\n");
  std::printf("\n%-6s %-8s %8s %10s %10s %12s %12s\n", "size", "backend",
              "threads", "wall_ms", "Mpx/s", "dispatch_ms", "compute_ms");

  for (int n : sizes) {
    const bool above = n >= gis::detail::kParallelGradientMinRows &&
                       n * n >= gis::detail::kParallelGradientMinPixels;
    const int modes[2] = {gis::detail::kGradientDispatchSerial,
                          gis::detail::kGradientDispatchParallel};
    const int nmodes = above ? 2 : 1;
    for (int mi = 0; mi < nmodes; ++mi) {
      const int mode = above ? modes[mi] : gis::detail::kGradientDispatchAuto;
      const CaseResult r = run_case(n, mode);
      rows.push_back(r);
      std::printf(" %-5d %-8s %8d %10.4f %10.4f %12.4f %12.4f\n", r.size,
                  r.backend, r.threads, r.wall_ms, r.mpx_s, r.dispatch_ms,
                  r.compute_ms);
    }
  }

  const auto root = artifact_root();
  write_json(root / "log" / "dem_gradient_bench.json", rows);
  write_json(root / "captures" / "analysis" / "dem_gradient" /
                 "dem_gradient_bench.json",
             rows);
  return 0;
}
