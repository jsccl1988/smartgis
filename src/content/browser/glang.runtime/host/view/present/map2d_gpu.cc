// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/view/present/map2d_gpu.h"

#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/bind/shell.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/map2d/profile/map2d_phase_profile.h"
#include "render/rhi/rhi.h"

#include "base/core/log.h"

#include <chrono>
#include <windows.h>

namespace content {
namespace detail {
namespace {

void log_phase(const char* tag) {
  const content::Map2dPhaseSample s = content::map2d_last_phase_sample();
  LOGGING(LOG_INFO,
          "map2d-showcase: %s layout_ms=%lld hillshade_ms=%lld "
          "bmp_io_ms=%lld gpu_upload_ms=%lld gpu_present_ms=%lld",
          tag, static_cast<long long>(s.layout_ms),
          static_cast<long long>(s.hillshade_ms),
          static_cast<long long>(s.bmp_io_ms),
          static_cast<long long>(s.gpu_upload_ms),
          static_cast<long long>(s.gpu_present_ms));
}

HWND create_showcase_gpu_hwnd(int showcase_w, int showcase_h) {
  static ATOM atom = 0;
  if (!atom) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SmartGisMap2dShowcaseGpu";
    atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return nullptr;
    }
  }
  const int w = showcase_w > 0 ? showcase_w : 1280;
  const int h = showcase_h > 0 ? showcase_h : 720;
  return CreateWindowExW(WS_EX_TOOLWINDOW, L"SmartGisMap2dShowcaseGpu",
                         L"map2d-scenario-gpu", WS_POPUP, 0, 0, w, h, nullptr,
                         nullptr, GetModuleHandleW(nullptr), nullptr);
}

render::rhi::Device* acquire_device(content::Shell& browser,
                                    int showcase_w,
                                    int showcase_h,
                                    bool* out_owned) {
  *out_owned = false;
  if (content::Pane* pane = browser.draw_host()) {
    if (pane->attach_mode() == content::Pane::AttachMode::kGpuPresent &&
        pane->rhi_device()) {
      auto* device = static_cast<render::rhi::Device*>(pane->rhi_device());
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
  write_mark(kMap2dMarkLeaf, "gpu-create", false);
  render::rhi::Device* device =
      render::rhi::create_device(render::rhi::preferred_gpu_backend());
  if (!device) {
    write_mark(kMap2dMarkLeaf, "gpu-create-fail", false);
    return nullptr;
  }
  render::rhi::DeviceDesc desc;
  desc.width = showcase_w;
  desc.height = showcase_h;
  // One flip-model chain per HWND. ContentMapView already owns the live
  // chain, so the smoke device gets a hidden popup.
  HWND present_hwnd = create_showcase_gpu_hwnd(showcase_w, showcase_h);
  if (!present_hwnd) {
    write_mark(kMap2dMarkLeaf, "gpu-hwnd-fail", false);
    device->shutdown();
    return nullptr;
  }
  write_mark(kMap2dMarkLeaf, "gpu-hwnd", false);
  desc.native_window = present_hwnd;
  write_mark(kMap2dMarkLeaf, "gpu-init", false);
  if (!device->initialize(desc)) {
    write_mark(kMap2dMarkLeaf, "gpu-init-fail", false);
    device->shutdown();
    return nullptr;
  }
  write_mark(kMap2dMarkLeaf, "gpu-device-ok", false);
  *out_owned = true;
  return device;
}

void sample_present(content::Map2dPresenter* map2d,
                    render::rhi::Device* device,
                    int showcase_w,
                    int showcase_h,
                    const char* ok_mark,
                    const char* fail_mark,
                    const char* phase_tag,
                    const char* log_fmt) {
  content::reset_map2d_phase_sample();
  const auto t0 = std::chrono::steady_clock::now();
  const bool ok = map2d->present_gpu(device, static_cast<uint32_t>(showcase_w),
                                     static_cast<uint32_t>(showcase_h));
  const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - t0)
                           .count();
  LOGGING(LOG_INFO, log_fmt, ok ? 1 : 0, ms);
  log_phase(phase_tag);
  write_mark(kMap2dMarkLeaf, ok ? ok_mark : fail_mark, false);
}

}  // namespace

void run_map2d_showcase_gpu(content::Shell& browser,
                            content::Map2dPresenter* map2d,
                            int showcase_w,
                            int showcase_h) {
  if (!map2d) {
    return;
  }
  write_mark(kMap2dMarkLeaf, "gpu-try", false);
  if (map2d->hosts_scenic_present()) {
    write_mark(kMap2dMarkLeaf, "gpu-present-enter", false);
    sample_present(map2d, nullptr, showcase_w, showcase_h, "gpu-present-ok",
                   "gpu-present-fail", "phase_cold_present",
                   "map2d-showcase: present_gpu=%d present_gpu_cold_ms=%lld "
                   "scenic=1");
    sample_present(map2d, nullptr, showcase_w, showcase_h, "gpu-warm-ok",
                   "gpu-warm-fail", "phase_warm_present",
                   "map2d-showcase: present_gpu_warm=%d "
                   "present_gpu_warm_ms=%lld scenic=1");
    return;
  }
  bool owned = false;
  render::rhi::Device* device =
      acquire_device(browser, showcase_w, showcase_h, &owned);
  if (!device) {
    write_mark(kMap2dMarkLeaf, "gpu-skip", false);
    LOGGING(LOG_INFO, "map2d-showcase: present_gpu skipped (no device)");
    return;
  }
  write_mark(kMap2dMarkLeaf, "gpu-present-enter", false);
  LOGGING(LOG_INFO, "map2d-showcase: gpu device acquired owned=%d",
          owned ? 1 : 0);
  sample_present(map2d, device, showcase_w, showcase_h, "gpu-present-ok",
                 "gpu-present-fail", "phase_cold_present",
                 "map2d-showcase: present_gpu=%d present_gpu_cold_ms=%lld");
  sample_present(map2d, device, showcase_w, showcase_h, "gpu-warm-ok",
                 "gpu-warm-fail", "phase_warm_present",
                 "map2d-showcase: present_gpu_warm=%d present_gpu_warm_ms=%lld");
  if (owned) {
    device->shutdown();
  }
}

}  // namespace detail
}  // namespace content
