// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_PRESENT_PRESENT_GPU_WARMUP_H_
#define APP_VIEWS_HARNESS_COMMON_PRESENT_PRESENT_GPU_WARMUP_H_

#include "app/views/harness/common/present/rhi_present_session.h"

#include <cstdint>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {
namespace detail {

// Scene-specific cleanup when present_gpu fails mid-warmup (overlays, FlyCube
// shutdown policy, detach_maps). Common does not decide those.
using PresentGpuWarmupFailFn = void (*)(int failed_frame, void* user);

// Multi-frame present_gpu + optional pump. Does not own globe fly / linger /
// BMP capture (those stay in scene present_run).
struct PresentGpuWarmupOpts {
  uint32_t width_px = 640;
  uint32_t height_px = 480;
  int frames = 1;
  int pump_ms = 50;
  // If set, logs "<prefix>: present_gpu failed frame N".
  const char* fail_log_prefix = nullptr;
  ShowcaseMarkFn mark = nullptr;
  // Used when |numbered_frame_marks| is false.
  const char* frame_mark = nullptr;
  // When true, mark("present-%d") each frame.
  bool numbered_frame_marks = false;
  PresentGpuWarmupFailFn on_fail = nullptr;
  void* on_fail_user = nullptr;
};

// Returns 0, or 52 if cam/device is null or present_gpu fails.
int present_gpu_warmup(content::Scene3dPresenter* cam,
                       render::rhi::Device* device,
                       const PresentGpuWarmupOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_PRESENT_PRESENT_GPU_WARMUP_H_
