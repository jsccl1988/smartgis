// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/capture.h"

#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/runtime/host/capability/shell.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/map2d_presenter.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "base/process/switches.h"

namespace plugin {
namespace detail {
namespace {

void log_map2d_phase_sample(const char* tag) {
  const content::Map2dPhaseSample s = content::map2d_last_phase_sample();
  std::fprintf(stderr,
               "map2d-showcase: %s layout_ms=%lld hillshade_ms=%lld "
               "software_paint_ms=%lld paint_ms=%lld bmp_io_ms=%lld "
               "gpu_upload_ms=%lld gpu_present_ms=%lld\n",
               tag, static_cast<long long>(s.layout_ms),
               static_cast<long long>(s.hillshade_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.bmp_io_ms),
               static_cast<long long>(s.gpu_upload_ms),
               static_cast<long long>(s.gpu_present_ms));
}

bool want_export_reuse() {
  if (const char* env = base::switch_cstr("map2d-export-reuse")) {
    return env[0] == '1' && env[1] == '\0';
  }
  return false;
}

}  // namespace

int prepare_map2d_capture_paths(const char* mode_name, Map2dCapturePaths* out) {
  if (!mode_name || !out) {
    return 56;
  }
  *out = Map2dCapturePaths{};
  char leaf_a[64] = {};
  std::snprintf(leaf_a, sizeof(leaf_a), "map2d-showcase-%s.bmp", mode_name);
  wchar_t leaf_w[64] = {};
  MultiByteToWideChar(CP_ACP, 0, leaf_a, -1, leaf_w, 64);
  HarnessShell* shell = map2d_scenario_shell();
  if (!shell || !shell->capture_path(out->bmp_w, MAX_PATH, leaf_w)) {
    std::fprintf(stderr, "map2d-showcase: sidecar path failed\n");
    return 56;
  }
  // fopen_s in export_bmp expects ACP, not UTF-8 �?keep ANSI sidecar.
  if (!shell->capture_path_a(out->bmp_a, MAX_PATH, leaf_a)) {
    std::fprintf(stderr, "map2d-showcase: sidecar path_a failed\n");
    return 56;
  }
  std::fprintf(stderr, "map2d-showcase: bmp path=%s\n", out->bmp_a);
  map2d_mark("bmp-path");
  DeleteFileW(out->bmp_w);
  map2d_mark("bmp-cleared");
  return 0;
}

int export_map2d_showcase_bmp(content::Map2dPresenter* map2d,
                              const Map2dCapturePaths& paths,
                              int showcase_w,
                              int showcase_h) {
  if (!map2d) {
    return 56;
  }
  // Bench: MAP2D_EXPORT_REUSE=1 warms present-cache off-clock, then the
  // timed export reports paint_ms (blit) vs export_ms (paint + bmp_io).
  if (want_export_reuse()) {
    map2d_mark("export-warm");
    char warm_a[MAX_PATH] = {};
    HarnessShell* shell = map2d_scenario_shell();
    if (shell &&
        shell->capture_path_a(warm_a, MAX_PATH, "map2d-showcase-export-warm.bmp")) {
      (void)map2d->export_bmp(warm_a, showcase_w, showcase_h);
    }
  }
  content::reset_map2d_phase_sample();
  const auto t0 = std::chrono::steady_clock::now();
  if (!map2d->export_bmp(paths.bmp_a, showcase_w, showcase_h)) {
    std::fprintf(stderr, "map2d-showcase: export_bmp failed\n");
    map2d_mark("bmp-fail");
    return 56;
  }
  const long long export_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - t0)
          .count();
  const content::Map2dPhaseSample export_ph = content::map2d_last_phase_sample();
  std::fprintf(stderr,
               "map2d-showcase: export_ms=%lld paint_ms=%lld bmp_io_ms=%lld\n",
               export_ms, static_cast<long long>(export_ph.software_paint_ms),
               static_cast<long long>(export_ph.bmp_io_ms));
  log_map2d_phase_sample("phase_export");
  map2d_mark("bmp-wrote");
  return 0;
}

int verify_map2d_showcase_bmp(const char* mode_name,
                              const Map2dCapturePaths& paths) {
  int bw = 0;
  int bh = 0;
  // export_bmp writes BI_RGB 32bpp (BGRA); default check rejects non-24bpp.
  HarnessShell* shell = map2d_scenario_shell();
  if (!shell || !shell->bmp_has_visible_signal(paths.bmp_a, &bw, &bh)) {
    std::fprintf(stderr, "map2d-showcase: BMP lacks visible signal (%dx%d)\n",
                 bw, bh);
    map2d_mark("bmp-black");
    return 54;
  }
  std::fprintf(stderr, "map2d-showcase: wrote %s (%dx%d)\n", paths.bmp_a, bw,
               bh);
  map2d_mark("bmp-ok");

  // Durable copy: multi-agent harness loops often DeleteFile the canonical
  // leaf between bmp-ok and human inspection. Keep a sibling that loops do
  // not target.
  if (mode_name) {
    wchar_t keep_w[MAX_PATH] = {};
    char keep_leaf[80] = {};
    std::snprintf(keep_leaf, sizeof(keep_leaf), "map2d-showcase-%s.keep.bmp",
                  mode_name);
    wchar_t keep_leaf_w[80] = {};
    MultiByteToWideChar(CP_ACP, 0, keep_leaf, -1, keep_leaf_w, 80);
    if (shell->capture_path(keep_w, MAX_PATH, keep_leaf_w)) {
      if (CopyFileW(paths.bmp_w, keep_w, FALSE)) {
        map2d_mark("bmp-keep");
      }
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace plugin
