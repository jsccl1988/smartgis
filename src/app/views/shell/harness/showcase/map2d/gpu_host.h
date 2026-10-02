// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_GPU_HOST_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_GPU_HOST_H_

#include <windows.h>

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {

class Browser;

namespace detail {

bool map2d_want_gpu_present();

// Hidden owner for a dedicated FlyCube swapchain when Map Edit is already on
// ContentMapView (DXGI allows one flip-model chain per HWND).
HWND create_map2d_showcase_gpu_hwnd(int showcase_w, int showcase_h);

// Create one Device for cold+warm samples (showcase size). Prefer a dedicated
// init at export pixels so HWND client size does not force swapchain churn.
render::rhi::Device* acquire_map2d_showcase_gpu_device(Browser& browser,
                                                       int showcase_w,
                                                       int showcase_h,
                                                       bool* out_owned);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_MAP2D_GPU_HOST_H_
