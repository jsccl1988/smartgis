// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/present/present_host.h"

#include "app/views/shell/harness/common/present/present_hwnd.h"

namespace app {
namespace detail {

HWND create_plugin_showcase_hwnd(uint32_t width_px, uint32_t height_px) {
  ShowcasePresentHwndOpts opts;
  opts.class_name = L"SmartGisPluginShowcase";
  opts.window_title = L"SmartGIS Plugin Showcase";
  opts.width_px = width_px;
  opts.height_px = height_px;
  return create_showcase_present_hwnd(opts);
}

}  // namespace detail
}  // namespace app
