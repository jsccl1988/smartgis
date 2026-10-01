// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID_H_

namespace app {

class Browser;

namespace detail {

// Map2d baogrid.create_orth_grid + unit-square BMP.
int run_orthogrid(Browser& browser);

// Scene3D orthogrid3d.create_hex_grid → overlay TIN + .vts + HWND BMP.
int run_orthogrid3d(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_ORTHOGRID_H_
