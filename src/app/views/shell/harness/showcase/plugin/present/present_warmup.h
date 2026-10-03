// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

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

// Four (or |frame_count|) present_gpu frames. Returns 0 or 52.
int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count = 4);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WARMUP_H_
