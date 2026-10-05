// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/capture/scene3d_capture.h"

#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"

#include <cstdio>

namespace app {
namespace detail {
namespace {

void mark_step(ShowcaseMarkFn mark, const char* step) {
  if (mark && step) {
    mark(step);
  }
}

void wait_before_capture(HWND /*hwnd*/, int pump_ms, bool sleep_instead) {
  if (pump_ms <= 0) {
    return;
  }
  if (sleep_instead) {
    Sleep(static_cast<DWORD>(pump_ms));
  } else {
    pump_messages(static_cast<DWORD>(pump_ms));
  }
}

CaptureOpts make_capture_opts(const Scene3dHwndCaptureOpts& opts) {
  CaptureOpts capture;
  if (opts.use_grid_lit_policy) {
    capture.max_attempts = 5;
    capture.pump_base_ms = 60;
    capture.pump_step_ms = 40;
    capture.visible = VisiblePolicy::kGridLitFraction;
  }
  return capture;
}

}  // namespace

bool capture_scene3d_hwnd_bmp(content::Scene3dPresenter* cam,
                              render::rhi::Device* device,
                              HWND capture_hwnd,
                              bool want_gpu,
                              const Scene3dHwndCaptureOpts& opts) {
  if (!want_gpu && opts.skip_when_null_gpu) {
    mark_step(opts.mark, opts.mark_skip_null);
    return true;
  }
  if (!cam || !opts.bmp_leaf) {
    mark_step(opts.mark, opts.mark_path_fail);
    return !want_gpu;
  }
  if (!device && !cam->hosts_scenic_present()) {
    mark_step(opts.mark, opts.mark_path_fail);
    return !want_gpu;
  }
  if (!capture_hwnd) {
    mark_step(opts.mark, opts.mark_skip);
    return !want_gpu;
  }

  wchar_t bmp_path[MAX_PATH] = {};
  if (!exe_capture_path(bmp_path, MAX_PATH, opts.bmp_leaf)) {
    mark_step(opts.mark, opts.mark_path_fail);
    return false;
  }

  if (!opts.skip_ui_thread_present) {
    (void)cam->present_gpu(device, opts.present_w, opts.present_h);
  }
  // Scenic GDI does not stick in the present HWND (no WM_PAINT owner-draw).
  // Export via scenic::Engine memory DIB instead of PrintWindow.
  if (!opts.skip_ui_thread_present && cam->hosts_scenic_present()) {
    char utf8[MAX_PATH * 3] = {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, bmp_path, -1, utf8,
                                      static_cast<int>(sizeof(utf8)), nullptr,
                                      nullptr);
    if (n > 0 &&
        cam->export_bmp(utf8, static_cast<int>(opts.present_w),
                        static_cast<int>(opts.present_h))) {
      int bw = 0;
      int bh = 0;
      BmpFileCheckOpts check;
      check.require_color_diversity = opts.require_color_diversity;
      // scenic::Engine::export_bmp writes 32bpp top-down DIBs.
      check.allow_32bpp = true;
      const bool signal =
          bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
      std::fwprintf(stderr, L"%S: wrote %ls (%dx%d signal=%d scenic_export=1)\n",
                    opts.log_prefix ? opts.log_prefix : "scene3d-capture",
                    bmp_path, bw, bh, signal ? 1 : 0);
      if (signal) {
        mark_step(opts.mark, opts.mark_ok);
        if (opts.after_ok) {
          (void)opts.after_ok(bmp_path, bw, bh, opts.after_ok_user);
        }
        return true;
      }
      mark_step(opts.mark, opts.mark_black);
      return !want_gpu;
    }
  }
  if (!opts.skip_ui_thread_present && cam->hosts_scenic_present() &&
      capture_hwnd && IsWindow(capture_hwnd)) {
    HDC dc = GetDC(capture_hwnd);
    if (dc) {
      cam->paint(dc, static_cast<int>(opts.present_w),
                 static_cast<int>(opts.present_h), true);
      ReleaseDC(capture_hwnd, dc);
    }
  }
  wait_before_capture(capture_hwnd, opts.pre_capture_pump_ms,
                      opts.sleep_instead_of_pump);

  const CaptureOpts capture = make_capture_opts(opts);
  if (!capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
    mark_step(opts.mark, opts.mark_skip);
    std::fprintf(stderr, "%s: HWND BMP capture failed\n",
                 opts.log_prefix ? opts.log_prefix : "scene3d-capture");
    return !want_gpu;
  }

  int bw = 0;
  int bh = 0;
  BmpFileCheckOpts check;
  check.require_color_diversity = opts.require_color_diversity;
  bool signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);

  if (!signal && opts.retry_dark_frame) {
    if (!opts.skip_ui_thread_present) {
      (void)cam->present_gpu(device, opts.present_w, opts.present_h);
      if (cam->hosts_scenic_present() && capture_hwnd &&
          IsWindow(capture_hwnd)) {
        HDC dc = GetDC(capture_hwnd);
        if (dc) {
          cam->paint(dc, static_cast<int>(opts.present_w),
                     static_cast<int>(opts.present_h), true);
          ReleaseDC(capture_hwnd, dc);
        }
      }
    }
    wait_before_capture(capture_hwnd, opts.pre_capture_pump_ms,
                        opts.sleep_instead_of_pump);
    if (capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
      signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
    }
  }

  std::fwprintf(stderr, L"%S: wrote %ls (%dx%d signal=%d)\n",
                opts.log_prefix ? opts.log_prefix : "scene3d-capture", bmp_path,
                bw, bh, signal ? 1 : 0);
  if (signal) {
    mark_step(opts.mark, opts.mark_ok);
    if (opts.after_ok) {
      (void)opts.after_ok(bmp_path, bw, bh, opts.after_ok_user);
    }
    return true;
  }
  mark_step(opts.mark, opts.mark_black);
  // Null-path best-effort capture: black frames do not fail the showcase.
  return !want_gpu;
}

}  // namespace detail
}  // namespace app
