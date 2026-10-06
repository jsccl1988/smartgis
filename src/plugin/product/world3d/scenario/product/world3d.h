// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD3D_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD3D_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Scene3D DEM + pointcloud overlay + HWND BMP capture.
int run_world3d_scene3d(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_WORLD3D_H_
