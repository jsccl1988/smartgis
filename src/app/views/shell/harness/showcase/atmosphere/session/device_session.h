// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_DEVICE_SESSION_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_DEVICE_SESSION_H_

#include "app/views/shell/harness/showcase/atmosphere/common/linger.h"

#include <cstdint>
#include <windows.h>

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace ui::views {
class DrawHost;
}  // namespace ui::views

namespace app {

class Browser;

namespace detail {

inline constexpr uint32_t kAtmosphereShowcaseW = 640;
inline constexpr uint32_t kAtmosphereShowcaseH = 480;

// Borrowed FlyCube swapchain is the Scene3d client, not 640×480. Present at
// HWND size; BMP capture scales down to kAtmosphereShowcaseW/H.
inline void atmosphere_hwnd_present_size(HWND hwnd, uint32_t* width_px,
                                         uint32_t* height_px) {
  uint32_t w = kAtmosphereShowcaseW;
  uint32_t h = kAtmosphereShowcaseH;
  RECT rc = {};
  if (hwnd && IsWindow(hwnd) && GetClientRect(hwnd, &rc)) {
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw >= 8 && ch >= 8) {
      w = static_cast<uint32_t>(cw);
      h = static_cast<uint32_t>(ch);
    }
  }
  if (width_px) {
    *width_px = w;
  }
  if (height_px) {
    *height_px = h;
  }
}

// Owned / borrowed RHI present surface for atmosphere showcase.
struct AtmosphereDeviceSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
  bool owns_device = false;
  bool borrowed_shell = false;
  ui::views::DrawHost* scene = nullptr;
  AtmosphereShowcaseLinger linger;
};

// Creates present HWND (GPU) or uses tab child (null), then initializes Device.
// On failure returns non-zero exit code and leaves |out| partially filled so
// the caller can DestroyWindow / detach_maps.
int prepare_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* out);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_DEVICE_SESSION_H_
