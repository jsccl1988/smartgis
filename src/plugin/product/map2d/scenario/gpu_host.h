// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_GPU_HOST_H_
#define PLUGIN_MAP2D_SCENARIO_GPU_HOST_H_

#include <windows.h>

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace plugin {
class HarnessShell;
namespace detail {

bool map2d_want_gpu_present();

// Hidden owner for a dedicated FlyCube swapchain when Map Edit is already on
// ContentMapView (DXGI allows one flip-model chain per HWND).
HWND create_map2d_showcase_gpu_hwnd(int showcase_w, int showcase_h);

// Create one Device for cold+warm samples (showcase size). Prefer a dedicated
// init at export pixels so HWND client size does not force swapchain churn.
render::rhi::Device* acquire_map2d_showcase_gpu_device(HarnessShell& browser,
                                                       int showcase_w,
                                                       int showcase_h,
                                                       bool* out_owned);

}  // namespace detail

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_GPU_HOST_H_
