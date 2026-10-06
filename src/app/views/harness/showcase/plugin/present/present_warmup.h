// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_

#include "app/views/harness/showcase/plugin/session/device_session.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Overlay / mesh / device policy when present_gpu fails mid-warmup.
struct PluginPresentFailPolicy {
  bool clear_pointcloud = true;
  bool clear_tin = true;
  bool abandon_mesh = true;
  bool shutdown_device = true;
};

// |frame_count| present_gpu frames. Returns 0 or 52.
// Bare world3d discards first (cold) and last (DXGI tail) from warm average.
// Overload with |perf_json_leaf| writes atmosphere-style present timing JSON
// under captures/plugin/ (ms_per_present + Scene3dPhaseSample fields).
int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count = 4);

int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count,
                                 const char* perf_json_leaf,
                                 const char* mode);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_
