// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_LINGER_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_LINGER_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "app/views/shell/harness/showcase/atmosphere/common/linger.h"

#include <windows.h>

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {
namespace detail {

// Result of the timed / until-close linger present loop.
struct AtmospherePresentLingerResult {
  bool bmp_signal_ok = false;
  int frames = 0;
};

// Interactive or timed linger: optional globe dive skim + late BMP capture.
AtmospherePresentLingerResult run_atmosphere_present_linger(
    AtmosphereShowcaseMode mode,
    const char* mode_name,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    render::rhi::Device* device,
    HWND present_hwnd,
    HWND owned_present_hwnd,
    const AtmosphereShowcaseLinger& linger,
    bool globe_flythrough,
    float china_yaw,
    float china_pitch,
    bool early_bmp_ok,
    int dumped_globe_frames);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_LINGER_H_
