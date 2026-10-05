// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_CAPTURE_H_

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace app {
namespace detail {

// HWND BMP capture knobs (world3d uses grid-lit + dark-frame retry).
struct PluginCaptureOpts {
  const wchar_t* bmp_leaf = nullptr;
  int pre_capture_pump_ms = 80;
  bool use_grid_lit_policy = false;
  bool retry_dark_frame = false;
};

// PrintWindow BMP of the borrowed shell Scene3D HWND (GPU FlyCube/RHI).
// Returns true when capture is accepted.
bool capture_plugin_hwnd_bmp(content::Scene3dPresenter* cam,
                             PluginDeviceSession* session,
                             const PluginCaptureOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_CAPTURE_H_
