// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_HOST_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_HOST_H_

#include <cstdint>
#include <windows.h>

namespace app {
namespace detail {

// Top-level present surface so FlyCube is not stuck on a tiny tab child HWND.
// Plugin-local peer of atmosphere/host (no cross-showcase link dependency).
HWND create_plugin_showcase_hwnd(uint32_t width_px, uint32_t height_px);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRESENT_HOST_H_
