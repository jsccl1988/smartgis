// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/session/gpu_host.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/showcase/map2d/common/progress.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <cstdlib>

namespace app {
namespace detail {

bool map2d_want_gpu_present() {
  if (const char* env = std::getenv("SMT_MAP2D_SHOWCASE_GPU")) {
    return env[0] == '1' && env[1] == '\0';
  }
  return false;
}

HWND create_map2d_showcase_gpu_hwnd(int showcase_w, int showcase_h) {
  static ATOM s_atom = 0;
  if (!s_atom) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SmartGisMap2dShowcaseGpu";
    s_atom = RegisterClassExW(&wc);
    if (!s_atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return nullptr;
    }
  }
  const int w = showcase_w > 0 ? showcase_w : 1280;
  const int h = showcase_h > 0 ? showcase_h : 720;
  return CreateWindowExW(
      WS_EX_TOOLWINDOW, L"SmartGisMap2dShowcaseGpu", L"map2d-showcase-gpu",
      WS_POPUP, 0, 0, w, h, nullptr, nullptr, GetModuleHandleW(nullptr),
      nullptr);
}

render::rhi::Device* acquire_map2d_showcase_gpu_device(Browser& browser,
                                                       int showcase_w,
                                                       int showcase_h,
                                                       bool* out_owned) {
  *out_owned = false;
  // Reuse MapViewport FlyCube device only when already warm at matching size.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube &&
        pane->rhi_device()) {
      auto* device = static_cast<render::rhi::Device*>(pane->rhi_device());
      // Prefer dedicated device when viewport size != showcase export size.
      RECT rc = {};
      if (pane->native_view() && IsWindow(pane->native_view())) {
        GetClientRect(pane->native_view(), &rc);
      }
      const int cw = rc.right > 0 ? rc.right : 0;
      const int ch = rc.bottom > 0 ? rc.bottom : 0;
      if (cw == showcase_w && ch == showcase_h) {
        return device;
      }
    }
  }
  map2d_showcase_mark("gpu-create");
  render::rhi::Device* device =
      render::rhi::create_device(render::rhi::preferred_gpu_backend());
  if (!device) {
    map2d_showcase_mark("gpu-create-fail");
    return nullptr;
  }
  render::rhi::DeviceDesc desc;
  desc.width = showcase_w;
  desc.height = showcase_h;
  // Never CreateSwapChainForHwnd on a ContentMapView / LocalDevice HWND --
  // that second flip-model chain AVs after initialize (mark stuck at gpu-try,
  // no BMP). Always use a dedicated hidden popup for the owned smoke device.
  HWND present_hwnd =
      create_map2d_showcase_gpu_hwnd(showcase_w, showcase_h);
  if (!present_hwnd) {
    map2d_showcase_mark("gpu-hwnd-fail");
    device->shutdown();
    return nullptr;
  }
  map2d_showcase_mark("gpu-hwnd");
  desc.native_window = present_hwnd;
  map2d_showcase_mark("gpu-init");
  if (!device->initialize(desc)) {
    map2d_showcase_mark("gpu-init-fail");
    device->shutdown();
    // Intentionally leak Device* -- FlyCube teardown policy.
    return nullptr;
  }
  map2d_showcase_mark("gpu-device-ok");
  *out_owned = true;
  return device;
}

}  // namespace detail
}  // namespace app
