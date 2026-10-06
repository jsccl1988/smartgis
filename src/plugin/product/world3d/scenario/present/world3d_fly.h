// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRESENT_WORLD3D_FLY_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRESENT_WORLD3D_FLY_H_

#include "plugin/product/world3d/scenario/session/device_session.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace plugin {

class HarnessShell;

namespace detail {

// Result of the world3d cinematic globe fly (space→clouds→DEM→ocean).
struct World3dGlobeFlyResult {
  int presents_added = 0;
  int stage_bmps_ok = 0;
};

// Present the four-beat flythrough and capture stage BMPs under
// captures/browser/ (space / clouds / dem / ocean). Parks orbit at high-China
// DEM hold for the suite score BMP.
World3dGlobeFlyResult run_world3d_globe_fly_presents(
    HarnessShell& browser,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    PluginDeviceSession* session);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRESENT_WORLD3D_FLY_H_
