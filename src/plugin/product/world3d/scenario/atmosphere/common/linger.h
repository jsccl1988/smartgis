// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LINGER_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LINGER_H_

#include <windows.h>

namespace plugin {
namespace detail {

// Linger policy for GPU showcase.
// GPU always stays until the present HWND is closed (sticky LINGER_MS must not
// auto-kill the window). Opt-in timed CI: ATMOSPHERE_SHOWCASE_TIMED_MS>0.
// ATMOSPHERE_SHOWCASE_LINGER_MS=0 skips linger (capture-only).
struct AtmosphereShowcaseLinger {
  bool until_close = false;
  DWORD ms = 0;
};

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_LINGER_H_
