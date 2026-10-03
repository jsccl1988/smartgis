// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_SESSION_FINISH_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_SESSION_FINISH_H_

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {
namespace detail {

// Teardown after capture (or early exit). Does not detach_maps.
struct PluginTeardownOpts {
  bool clear_pointcloud = false;
  bool clear_tin = false;
  bool abandon_mesh = false;
  bool shutdown_device = true;
  bool destroy_owned_hwnd = true;
};

// Clear overlays / abandon / shutdown / DestroyWindow per |opts|.
void teardown_plugin_device_session(content::Scene3dPresenter* cam,
                                    PluginDeviceSession* session,
                                    const PluginTeardownOpts& opts);

// Destroy owned HWND only (device pointer intentionally leaked / kept).
void destroy_plugin_owned_hwnd(PluginDeviceSession* session);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_SESSION_FINISH_H_
