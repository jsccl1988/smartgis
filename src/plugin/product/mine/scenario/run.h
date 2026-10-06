// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_MINE_SCENARIO_RUN_H_
#define PLUGIN_PRODUCT_MINE_SCENARIO_RUN_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Scene3D mine TIN + borehole sticks + HWND BMP capture.
int run_mine_scene3d(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_MINE_SCENARIO_RUN_H_
