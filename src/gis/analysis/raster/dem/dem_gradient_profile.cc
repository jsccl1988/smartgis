// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/dem_gradient_profile.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "base/util/path.h"

namespace gis {
namespace detail {
namespace {

std::atomic<int> g_dispatch_override{kGradientDispatchAuto};
DemGradientProfile g_last_profile;
std::mutex g_last_profile_mu;

std::filesystem::path artifact_root() {
  const std::string exe = base::self_path();
  if (!exe.empty()) {
    return std::filesystem::path(exe);
  }
  if (const char* e = std::getenv("SMARTGIS_OUT"); e && e[0]) {
    return std::filesystem::path(e);
  }
  return std::filesystem::current_path();
}

void write_profile_json(const std::filesystem::path& path,
                        const DemGradientProfile& snap) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  FILE* f = nullptr;
#if defined(_MSC_VER)
  fopen_s(&f, path.string().c_str(), "wb");
#else
  f = std::fopen(path.string().c_str(), "wb");
#endif
  if (!f) {
    return;
  }
  std::fprintf(f,
               "{\n"
               "  \"kernel\": \"dem_gradient\",\n"
               "  \"min_pixels\": %d,\n"
               "  \"min_rows\": %d,\n"
               "  \"width\": %d,\n"
               "  \"height\": %d,\n"
               "  \"pixels\": %d,\n"
               "  \"threads\": %d,\n"
               "  \"dispatch_ms\": %.6f,\n"
               "  \"compute_ms\": %.6f,\n"
               "  \"backend\": \"%s\"\n"
               "}\n",
               kParallelGradientMinPixels, kParallelGradientMinRows, snap.width,
               snap.height, snap.pixels, snap.threads, snap.dispatch_ms,
               snap.compute_ms, snap.backend ? snap.backend : "serial");
  std::fclose(f);
}

}  // namespace

bool dem_gradient_profile_enabled() {
  const char* dump = std::getenv("ANALYSIS_PROFILE_DUMP");
  if (dump && dump[0]) {
    return true;
  }
  const char* e = std::getenv("ANALYSIS_PROFILE");
  if (!e || !e[0]) {
    return false;
  }
  return std::strcmp(e, "1") == 0 || std::strcmp(e, "true") == 0 ||
         std::strcmp(e, "TRUE") == 0 || std::strcmp(e, "on") == 0 ||
         std::strcmp(e, "ON") == 0;
}

void set_dem_gradient_dispatch_override(int mode) {
  g_dispatch_override.store(mode, std::memory_order_relaxed);
}

int dem_gradient_dispatch_override() {
  return g_dispatch_override.load(std::memory_order_relaxed);
}

DemGradientProfile last_dem_gradient_profile() {
  std::lock_guard<std::mutex> lock(g_last_profile_mu);
  return g_last_profile;
}

void record_dem_gradient_profile(const DemGradientProfile& snap) {
  {
    std::lock_guard<std::mutex> lock(g_last_profile_mu);
    g_last_profile = snap;
  }
  if (!dem_gradient_profile_enabled()) {
    return;
  }
  std::fprintf(stderr,
               "dem_gradient %dx%d pixels=%d threads=%d dispatch_ms=%.4f "
               "compute_ms=%.4f backend=%s\n",
               snap.width, snap.height, snap.pixels, snap.threads,
               snap.dispatch_ms, snap.compute_ms,
               snap.backend ? snap.backend : "serial");
  if (const char* dump = std::getenv("ANALYSIS_PROFILE_DUMP");
      dump && dump[0]) {
    write_profile_json(std::filesystem::path(dump), snap);
    return;
  }
  const auto root = artifact_root();
  write_profile_json(root / "log" / "dem_gradient_profile.json", snap);
  write_profile_json(root / "captures" / "analysis" / "dem_gradient" /
                         "last.json",
                     snap);
}

}  // namespace detail
}  // namespace gis
