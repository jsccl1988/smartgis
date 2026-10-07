// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/shot/scene_capture.h"

#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/view/pixel/gate.h"
#include "app/views/il.runtime/backend/view/dib/gdi.h"
#include "app/views/il.runtime/backend/view/dib/read.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/util/charset.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace app {
namespace detail {
namespace {

bool write_bottom_up_bgr24_bmp(const wchar_t* path, int width, int height,
                               const unsigned char* packed_bgr) {
  if (!path || !packed_bgr || width < 8 || height < 8) {
    return false;
  }
  const int stride = bgr24_stride(width);
  const size_t nbytes =
      static_cast<size_t>(stride) * static_cast<size_t>(height);
  std::vector<unsigned char> padded(nbytes, 0);
  for (int y = 0; y < height; ++y) {
    const unsigned char* src =
        packed_bgr + static_cast<size_t>(y) * static_cast<size_t>(width) * 3u;
    unsigned char* dst = padded.data() + static_cast<size_t>(y) * stride;
    std::memcpy(dst, src, static_cast<size_t>(width) * 3u);
  }
  BITMAPINFOHEADER header = bgr24_header(width, height);
  // Packed buffer from stereo_hwnd_capture_bgr24 is bottom-up (positive
  // biHeight), matching BMP/glReadPixels convention.
  header.biHeight = height;
  return write_bmp_file(path, header, padded.data(), padded.size());
}

void wait_before_capture(int pump_ms, bool sleep_instead) {
  if (pump_ms <= 0) {
    return;
  }
  if (sleep_instead) {
    Sleep(static_cast<DWORD>(pump_ms));
  } else {
    pump_messages(static_cast<DWORD>(pump_ms));
  }
}

void paint_scenic_hwnd(content::Scene3dPresenter* cam,
                       HWND hwnd,
                       uint32_t w,
                       uint32_t h) {
  if (!cam || !hwnd || !IsWindow(hwnd)) {
    return;
  }
  HDC dc = GetDC(hwnd);
  if (!dc) {
    return;
  }
  cam->paint(dc, static_cast<int>(w), static_cast<int>(h), true);
  ReleaseDC(hwnd, dc);
}

void present_then_paint(content::Scene3dPresenter* cam,
                        render::rhi::Device* device,
                        HWND hwnd,
                        const Scene3dHwndCaptureOpts& opts) {
  if (opts.skip_ui_thread_present) {
    return;
  }
  (void)present_scene3d_gpu(cam, device, opts.present_w, opts.present_h);
  if (cam->hosts_scenic_present()) {
    paint_scenic_hwnd(cam, hwnd, opts.present_w, opts.present_h);
  }
}

CaptureOpts make_capture_opts(const Scene3dHwndCaptureOpts& opts) {
  CaptureOpts capture;
  capture.dst_w = opts.dst_w;
  capture.dst_h = opts.dst_h;
  if (opts.use_grid_lit_policy) {
    capture.max_attempts = 5;
    capture.pump_base_ms = 60;
    capture.pump_step_ms = 40;
    capture.visible = VisiblePolicy::kGridLitFraction;
  }
  return capture;
}

bool finish_ok(const Scene3dHwndCaptureOpts& opts,
               const wchar_t* bmp_path,
               int bw,
               int bh) {
  if (opts.engine_sidecar) {
    (void)write_engine_sidecar(bmp_path, opts.engine_sidecar);
  }
  mark_step(opts.mark, opts.mark_ok);
  if (opts.after_ok) {
    (void)opts.after_ok(bmp_path, bw, bh, opts.after_ok_user);
  }
  return true;
}

void log_wrote(const Scene3dHwndCaptureOpts& opts,
               const wchar_t* bmp_path,
               int bw,
               int bh,
               bool signal,
               bool scenic_export) {
  const char* prefix = opts.log_prefix ? opts.log_prefix : "scene3d-capture";
  if (scenic_export) {
    std::fwprintf(stderr, L"%S: wrote %ls (%dx%d signal=%d scenic_export=1)\n",
                  prefix, bmp_path, bw, bh, signal ? 1 : 0);
  } else {
    std::fwprintf(stderr, L"%S: wrote %ls (%dx%d signal=%d)\n", prefix, bmp_path,
                  bw, bh, signal ? 1 : 0);
  }
}

BmpFileCheckOpts bmp_check_opts(const Scene3dHwndCaptureOpts& opts,
                                bool force_32bpp) {
  BmpFileCheckOpts check;
  check.require_color_diversity = opts.require_color_diversity;
  check.allow_32bpp = force_32bpp || opts.allow_32bpp;
  check.mean_min = opts.mean_min;
  check.mean_max = opts.mean_max;
  return check;
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
  if (!cam || (!opts.bmp_path && !opts.bmp_leaf)) {
    mark_step(opts.mark, opts.mark_path_fail);
    return !want_gpu;
  }
  if (!opts.skip_ui_thread_present && !device &&
      !cam->hosts_scenic_present()) {
    mark_step(opts.mark, opts.mark_path_fail);
    return !want_gpu;
  }
  if (!capture_hwnd) {
    mark_step(opts.mark, opts.mark_skip);
    return !want_gpu;
  }

  wchar_t resolved[MAX_PATH] = {};
  const wchar_t* bmp_path = opts.bmp_path;
  if (!bmp_path) {
    if (!exe_capture_path(resolved, MAX_PATH, opts.bmp_leaf)) {
      mark_step(opts.mark, opts.mark_path_fail);
      return false;
    }
    bmp_path = resolved;
  }

  if (!opts.skip_ui_thread_present) {
    (void)present_scene3d_gpu(cam, device, opts.present_w, opts.present_h);
  }

  // Stereo GL/D3D: front-buffer / staging readback — HWND BitBlt sees desktop
  // through a swapchain with no GDI redirection bitmap.
  if (opts.gpu_readback) {
    const int rw = static_cast<int>(opts.present_w);
    const int rh = static_cast<int>(opts.present_h);
    std::vector<unsigned char> bgr(
        static_cast<size_t>(rw) * static_cast<size_t>(rh) * 3u);
    if (opts.gpu_readback(opts.gpu_readback_user, bgr.data(), rw, rh) &&
        write_bottom_up_bgr24_bmp(bmp_path, rw, rh, bgr.data())) {
      int bw = 0;
      int bh = 0;
      const BmpFileCheckOpts check = bmp_check_opts(opts, false);
      const bool signal =
          bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
      log_wrote(opts, bmp_path, bw, bh, signal, false);
      if (signal) {
        return finish_ok(opts, bmp_path, bw, bh);
      }
      mark_step(opts.mark, opts.mark_black);
      return !want_gpu;
    }
  }

  // Scenic GDI does not stick in the present HWND (no WM_PAINT owner-draw).
  // Export via scenic::Engine memory DIB instead of PrintWindow.
  const bool try_scenic_export =
      cam->hosts_scenic_present() &&
      (opts.prefer_scenic_export || !opts.skip_ui_thread_present);
  if (try_scenic_export) {
    // Borrowed shell skips UI-thread present_gpu; paint into the HWND so the
    // engine DIB has a fresh frame before export (scenic matrix rc=50).
    if (opts.skip_ui_thread_present) {
      paint_scenic_hwnd(cam, capture_hwnd, opts.present_w, opts.present_h);
    }
    char utf8[MAX_PATH * 3] = {};
    if (wide_to_utf8(bmp_path, utf8, sizeof(utf8)) &&
        cam->export_bmp(utf8, static_cast<int>(opts.present_w),
                        static_cast<int>(opts.present_h))) {
      int bw = 0;
      int bh = 0;
      // scenic::Engine::export_bmp writes 32bpp top-down DIBs.
      const BmpFileCheckOpts check = bmp_check_opts(opts, true);
      const bool signal =
          bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
      log_wrote(opts, bmp_path, bw, bh, signal, true);
      if (signal) {
        return finish_ok(opts, bmp_path, bw, bh);
      }
      mark_step(opts.mark, opts.mark_black);
      return !want_gpu;
    }
  }
  if (!opts.skip_ui_thread_present && cam->hosts_scenic_present()) {
    paint_scenic_hwnd(cam, capture_hwnd, opts.present_w, opts.present_h);
  }
  wait_before_capture(opts.pre_capture_pump_ms, opts.sleep_instead_of_pump);

  const CaptureOpts capture = make_capture_opts(opts);
  if (!capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
    mark_step(opts.mark, opts.mark_skip);
    std::fprintf(stderr, "%s: HWND BMP capture failed\n",
                 opts.log_prefix ? opts.log_prefix : "scene3d-capture");
    return !want_gpu;
  }

  int bw = 0;
  int bh = 0;
  const BmpFileCheckOpts check = bmp_check_opts(opts, false);
  bool signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);

  if (!signal && opts.retry_dark_frame) {
    present_then_paint(cam, device, capture_hwnd, opts);
    wait_before_capture(opts.pre_capture_pump_ms, opts.sleep_instead_of_pump);
    if (capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
      signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
    }
  }

  log_wrote(opts, bmp_path, bw, bh, signal, false);
  if (signal) {
    return finish_ok(opts, bmp_path, bw, bh);
  }
  mark_step(opts.mark, opts.mark_black);
  // Null-path best-effort capture: black frames do not fail the run.
  return !want_gpu;
}

}  // namespace detail
}  // namespace app
