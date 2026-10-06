// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/session/host.h"

#include "plugin/product/world3d/scenario/common/host_rhi.h"

namespace plugin {
namespace detail {

HWND create_atmosphere_showcase_hwnd(uint32_t width_px, uint32_t height_px) {
  ShowcasePresentHwndOpts opts;
  opts.class_name = L"SmartGisAtmosphereShowcase";
  opts.window_title = L"SmartGIS Atmosphere Showcase";
  opts.width_px = width_px;
  opts.height_px = height_px;
  return create_showcase_present_hwnd(opts);
}

}  // namespace detail
}  // namespace plugin
