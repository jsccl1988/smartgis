// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_GLOBE_PRESENT_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_GLOBE_PRESENT_H_

#include "app/views/app/cmdline/views_launch_options.h"

#include <windows.h>

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace ui::views {
class DrawHost;
}  // namespace ui::views

namespace plugin {
namespace detail {

using AtmosphereShowcaseMode = ::app::AtmosphereShowcaseMode;

// Result of the cinematic globe fly-in present pass.
struct AtmosphereGlobeFlyResult {
  int presents_added = 0;
  bool early_bmp_ok = false;
  int dumped_frames = 0;
};

// Space → high-altitude fly, BMP at high beat, optional HARNESS_RECORD
// keyframe dump under captures/record/atmosphere_globe_fly/.
AtmosphereGlobeFlyResult run_atmosphere_globe_fly_presents(
    AtmosphereShowcaseMode mode,
    const char* mode_name,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    render::rhi::Device* device,
    HWND present_hwnd,
    HWND owned_present_hwnd,
    ui::views::DrawHost* scene,
    float china_yaw,
    float china_pitch);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_GLOBE_PRESENT_H_
