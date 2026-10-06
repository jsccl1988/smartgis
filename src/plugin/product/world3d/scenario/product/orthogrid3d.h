// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_ORTHOGRID3D_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_ORTHOGRID3D_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Scene3D orthogrid3d.create_hex_grid → surface orthogonal grid + .vts + HWND BMP.
int run_orthogrid3d(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_PRODUCT_ORTHOGRID3D_H_
