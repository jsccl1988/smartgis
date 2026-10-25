// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_VIEW_PRESENT_TIMED_PRESENT_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_VIEW_PRESENT_TIMED_PRESENT_H_

#include <cstdint>

namespace content {
class Pane;
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace content {
namespace detail {

// Options for a fair Scene3d timed-present window (phase-loop / matrix).
// When |frame_count| <= 0 the helper is a no-op (returns 0).
struct TimedPresentOpts {
  // Timed presents (1–600). Prefer ATMOSPHERE_SHOWCASE_PRESENT_COUNT /
  // PLUGIN_WORLD3D_PRESENT_COUNT / SCENE3D_PRESENT_COUNT via
  // resolve_timed_present_count().
  int frame_count = 0;
  // Message pump between frames. Skill: count>3 → force 0 (no map2d paint).
  int pump_ms = 0;
  // Warm average discards (bare matrix: cold+tail; atmosphere: cold only).
  int discard_cold = 1;
  int discard_tail = 0;
  int want_gpu = 1;
  // Locked equal-profile size (skill: 640×480). present_gpu path only.
  uint32_t width_px = 640;
  uint32_t height_px = 480;
  // captures/<scenario>/ leaf, e.g. atmosphere-showcase-perf.json
  const char* perf_json_leaf = nullptr;
  // JSON "mode" field (legacy / world3d-full / …).
  const char* mode = nullptr;
  // Prefer direct present_gpu at |width_px|×|height_px| when both set.
  // Falls back to shell Display present (full client) when null.
  content::Scene3dPresenter* cam = nullptr;
  render::rhi::Device* device = nullptr;
  // Wait budget per shell present (Display thread). 0 → present helper default.
  unsigned present_wait_ms = 800;
};

// Reads PRESENT_COUNT from product switches (0 if unset / invalid).
int resolve_timed_present_count();

// Times |opts.frame_count| Scene3d presents, writes phase perf JSON.
// Returns 0 on success, 52 if no frame advanced.
int run_timed_scene3d_presents(content::Pane* scene,
                               const TimedPresentOpts& opts);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_VIEW_PRESENT_TIMED_PRESENT_H_
