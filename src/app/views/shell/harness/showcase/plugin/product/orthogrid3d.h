// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID3D_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID3D_H_

namespace app {

class Browser;

namespace detail {

// Scene3D orthogrid3d.create_hex_grid → surface orthogonal grid + .vts + HWND BMP.
int run_orthogrid3d(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID3D_H_
