// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_DEVICE_SESSION_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_DEVICE_SESSION_H_

#include "app/views/shell/harness/showcase/atmosphere/linger.h"

#include <cstdint>
#include <windows.h>

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {

class Browser;

namespace detail {

inline constexpr uint32_t kAtmosphereShowcaseW = 640;
inline constexpr uint32_t kAtmosphereShowcaseH = 480;

// Owned / borrowed RHI present surface for atmosphere showcase.
struct AtmosphereDeviceSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
  bool owns_device = false;
  AtmosphereShowcaseLinger linger;
};

// Creates present HWND (GPU) or uses tab child (null), then initializes Device.
// On failure returns non-zero exit code and leaves |out| partially filled so
// the caller can DestroyWindow / detach_maps.
int prepare_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* out);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_DEVICE_SESSION_H_
