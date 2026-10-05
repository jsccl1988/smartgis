// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_CAPTURE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace ui::views {
class DrawHost;
}  // namespace ui::views

namespace app {
namespace detail {

// Present the live swapchain at HWND client size, then write a 640×480 BMP
// (scale-down so suite gates never score a full-shell HWND).
bool capture_atmosphere_named_bmp(content::Scene3dPresenter* cam,
                                  render::rhi::Device* device,
                                  HWND present_hwnd,
                                  HWND owned_present_hwnd,
                                  const wchar_t* bmp_leaf,
                                  bool globe_flythrough,
                                  ui::views::DrawHost* scene = nullptr);

// Present once, BitBlt HWND → atmosphere-showcase-<name>.bmp, gate visible
// signal. Legacy mode optionally composites GDI place-name labels onto the BMP.
// Returns true when the GPU visual gate passes (or when !want_gpu).
bool capture_atmosphere_showcase_bmp(AtmosphereShowcaseMode mode,
                                     const char* mode_name,
                                     content::Scene3dPresenter* cam,
                                     render::rhi::Device* device,
                                     HWND present_hwnd,
                                     HWND owned_present_hwnd,
                                     bool want_gpu,
                                     bool globe_flythrough,
                                     ui::views::DrawHost* scene = nullptr);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_CAPTURE_CAPTURE_H_
