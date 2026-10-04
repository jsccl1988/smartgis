// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/capture/capture.h"

#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/common/capture/scene3d_capture.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

void plugin_mark(const char* step) {
  plugin_showcase_mark(step);
}

bool write_dib_bmp(const wchar_t* path, int width_px, int height_px,
                   const void* bits, int stride_bytes) {
  if (!path || !bits || width_px <= 0 || height_px <= 0 || stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = -height_px;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
                  std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
                  std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
}

// Isolate SEH from C++ objects that need unwinding (MSVC C2712).
bool paint_mem_dc_seh(content::Scene3dPresenter* cam, HDC mem, int w, int h) {
  if (!cam || !mem || w <= 0 || h <= 0) {
    return false;
  }
  __try {
    cam->paint(mem, w, h, true);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// Showcase SoT: shaded/filled surface PLUS wireframe overlay so inspect
// BMP/PNG shows depth/topology (FlyCube HWND often collapses to AABB toys).
bool try_software_paint_bmp(content::Scene3dPresenter* cam,
                            const wchar_t* bmp_leaf) {
  if (!cam || !bmp_leaf) {
    return false;
  }
  cam->gpu().set_wireframe_enabled(true);
  wchar_t bmp_path[MAX_PATH] = {};
  if (!exe_capture_path(bmp_path, MAX_PATH, bmp_leaf)) {
    return false;
  }
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kPluginShowcasePresentW;
  bmi.bmiHeader.biHeight = -kPluginShowcasePresentH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  bool painted = paint_mem_dc_seh(cam, mem, kPluginShowcasePresentW,
                                  kPluginShowcasePresentH);
  if (!painted) {
    cam->paint(mem, kPluginShowcasePresentW, kPluginShowcasePresentH, true);
    painted = true;
  }
  SelectObject(mem, old);
  bool wrote = false;
  if (painted) {
    const int stride = kPluginShowcasePresentW * 4;
    wrote = write_dib_bmp(bmp_path, kPluginShowcasePresentW,
                          kPluginShowcasePresentH, bits, stride);
  }
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  if (!wrote) {
    return false;
  }
  int bw = 0;
  int bh = 0;
  BmpFileCheckOpts check;
  check.require_color_diversity = true;
  check.allow_32bpp = true;
  if (!bmp_file_has_visible_signal(bmp_path, &bw, &bh, check)) {
    return false;
  }
  std::fwprintf(stderr,
                L"plugin-showcase: wrote %ls (%dx%d software_paint=1)\n",
                bmp_path, bw, bh);
  plugin_mark("bmp-ok");
  return true;
}

}  // namespace

bool capture_plugin_hwnd_bmp(content::Scene3dPresenter* cam,
                             PluginDeviceSession* session,
                             const PluginCaptureOpts& opts) {
  if (!cam || !session) {
    return false;
  }
  // Prefer software solid+wireframe for visual-review BMPs. FlyCube HWND
  // often draws AABB stand-ins (stormsurge/mine) that pass lit gates but fail
  // product inspect. Warm GPU present first so mesh state stays live.
  if (session->device) {
    cam->gpu().set_wireframe_enabled(true);
    (void)cam->present_gpu(session->device, kPluginShowcasePresentW,
                           kPluginShowcasePresentH);
  }
  if (try_software_paint_bmp(cam, opts.bmp_leaf)) {
    return true;
  }
  plugin_mark("bmp-software-fail");

  Scene3dHwndCaptureOpts core;
  core.bmp_leaf = opts.bmp_leaf;
  core.present_w = kPluginShowcasePresentW;
  core.present_h = kPluginShowcasePresentH;
  core.pre_capture_pump_ms = opts.pre_capture_pump_ms;
  core.use_grid_lit_policy = opts.use_grid_lit_policy;
  core.retry_dark_frame = opts.retry_dark_frame;
  core.require_color_diversity = true;
  core.mark = plugin_mark;
  core.log_prefix = "plugin-showcase";

  cam->gpu().set_wireframe_enabled(true);
  if (capture_scene3d_hwnd_bmp(cam, session->device, session->present_hwnd,
                               session->want_gpu, core)) {
    return true;
  }
  plugin_mark("bmp-hwnd-fail");
  return false;
}

}  // namespace detail
}  // namespace app
