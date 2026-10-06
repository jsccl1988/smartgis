// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_HOST_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_HOST_H_

#include <cstdint>
#include <windows.h>

namespace plugin {
namespace detail {

// Top-level present surface so FlyCube is not stuck on a tiny tab child HWND.
HWND create_atmosphere_showcase_hwnd(uint32_t width_px, uint32_t height_px);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_HOST_H_
