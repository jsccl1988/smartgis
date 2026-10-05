// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WORLD3D_FLY_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WORLD3D_FLY_H_

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Result of the world3d cinematic globe fly (space→clouds→DEM→ocean).
struct World3dGlobeFlyResult {
  int presents_added = 0;
  int stage_bmps_ok = 0;
};

// Present the four-beat flythrough and capture stage BMPs under
// captures/plugin/ (space / clouds / dem / ocean). Parks orbit at high-China
// DEM hold for the suite score BMP.
World3dGlobeFlyResult run_world3d_globe_fly_presents(
    Browser& browser,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    PluginDeviceSession* session);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_WORLD3D_FLY_H_
