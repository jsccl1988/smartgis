// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_H_

namespace app {

class Browser;

namespace detail {

// Scene3D DEM + pointcloud overlay + HWND BMP capture.
int run_world3d_scene3d(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_WORLD3D_H_
