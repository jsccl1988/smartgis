// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/capture/capture.h"

#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"
#include "app/views/shell/harness/showcase/atmosphere/capture/label_composite.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdio>
#include <vector>

namespace app {
namespace detail {
namespace {

struct AtmosphereLabelHook {
  AtmosphereShowcaseMode mode;
  content::Scene3dPresenter* cam = nullptr;
};

bool atmosphere_after_ok(const wchar_t* bmp_path, int w, int h, void* user) {
  auto* hook = static_cast<AtmosphereLabelHook*>(user);
  if (!hook || hook->mode != AtmosphereShowcaseMode::kLegacy) {
    return false;
  }
  if (composite_legacy_labels_onto_bmp(hook->cam, bmp_path, w, h)) {
    atmosphere_showcase_mark("labels-bmp-ok");
    return true;
  }
  atmosphere_showcase_mark("labels-bmp-skip");
  return false;
}

bool write_bgr24_bmp(const wchar_t* filename, int w, int h,
                     const std::vector<unsigned char>& pixels, int stride) {
  if (!filename || w < 8 || h < 8 || pixels.empty()) {
    return false;
  }
  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = w;
  bi.biHeight = -h;
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());
  FILE* out = nullptr;
  if (_wfopen_s(&out, filename, L"wb") != 0 || !out) {
    return false;
  }
  std::fwrite(&fh, sizeof(fh), 1, out);
  std::fwrite(&bi, sizeof(bi), 1, out);
  std::fwrite(pixels.data(), 1, static_cast<size_t>(stride) * static_cast<size_t>(h),
              out);
  std::fclose(out);
  return true;
}

// Present into the live swapchain at client size, BitBlt, scale to dst_w×dst_h.
bool capture_hwnd_scaled_bmp(HWND hwnd, const wchar_t* filename, int dst_w,
                             int dst_h) {
  if (!hwnd || !IsWindow(hwnd) || !filename || dst_w < 8 || dst_h < 8) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int sw = rc.right - rc.left;
  const int sh = rc.bottom - rc.top;
  if (sw < 8 || sh < 8) {
    return false;
  }
  HDC wnd_dc = GetDC(hwnd);
  if (!wnd_dc) {
    return false;
  }
  HDC src_dc = CreateCompatibleDC(wnd_dc);
  HBITMAP src_bmp = CreateCompatibleBitmap(wnd_dc, sw, sh);
  HDC dst_dc = CreateCompatibleDC(wnd_dc);
  HBITMAP dst_bmp = CreateCompatibleBitmap(wnd_dc, dst_w, dst_h);
  if (!src_dc || !src_bmp || !dst_dc || !dst_bmp) {
    if (dst_bmp) {
      DeleteObject(dst_bmp);
    }
    if (dst_dc) {
      DeleteDC(dst_dc);
    }
    if (src_bmp) {
      DeleteObject(src_bmp);
    }
    if (src_dc) {
      DeleteDC(src_dc);
    }
    ReleaseDC(hwnd, wnd_dc);
    return false;
  }
  HGDIOBJ old_src = SelectObject(src_dc, src_bmp);
  HGDIOBJ old_dst = SelectObject(dst_dc, dst_bmp);
  // Screen blit of the target HWND (FlyCube DXGI popup). PrintWindow of a
  // flip-model swapchain is often white; overwriting with the desktop pixels
  // at this window's client origin is the working capture path.
  if (!blit_client_to_dib(hwnd, src_dc, sw, sh)) {
    BOOL printed =
        PrintWindow(hwnd, src_dc, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
    if (!printed) {
      printed = PrintWindow(hwnd, src_dc, PW_RENDERFULLCONTENT);
    }
    if (!printed) {
      (void)BitBlt(src_dc, 0, 0, sw, sh, wnd_dc, 0, 0, SRCCOPY);
    }
  }
  SetStretchBltMode(dst_dc, HALFTONE);
  SetBrushOrgEx(dst_dc, 0, 0, nullptr);
  StretchBlt(dst_dc, 0, 0, dst_w, dst_h, src_dc, 0, 0, sw, sh, SRCCOPY);

  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = dst_w;
  bi.biHeight = -dst_h;
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((dst_w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(dst_h));
  const int got = GetDIBits(dst_dc, dst_bmp, 0, dst_h, pixels.data(),
                            reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
  SelectObject(dst_dc, old_dst);
  SelectObject(src_dc, old_src);
  DeleteObject(dst_bmp);
  DeleteDC(dst_dc);
  DeleteObject(src_bmp);
  DeleteDC(src_dc);
  ReleaseDC(hwnd, wnd_dc);
  if (got != dst_h) {
    return false;
  }
  return write_bgr24_bmp(filename, dst_w, dst_h, pixels, stride);
}

}  // namespace

bool capture_atmosphere_named_bmp(content::Scene3dPresenter* cam,
                                  render::rhi::Device* device,
                                  HWND present_hwnd,
                                  HWND owned_present_hwnd,
                                  const wchar_t* bmp_leaf,
                                  bool globe_flythrough,
                                  ui::views::DrawHost* scene) {
  HWND hwnd = owned_present_hwnd ? owned_present_hwnd : present_hwnd;
  if (scene) {
    // DXGI FlyCube popup only. native_view() is often behind the Map tab;
    // screen-BitBlt of that rect captures the 2D GDI/Skia pane.
    if (HWND live = scene->present_hwnd()) {
      if (IsWindow(live)) {
        hwnd = live;
      }
    }
  }
  if (!cam || !device || !bmp_leaf || !hwnd || !IsWindow(hwnd)) {
    return false;
  }
  RECT rc = {};
  int cw = static_cast<int>(kAtmosphereShowcaseW);
  int ch = static_cast<int>(kAtmosphereShowcaseH);
  if (GetClientRect(hwnd, &rc)) {
    const int rw = rc.right - rc.left;
    const int rh = rc.bottom - rc.top;
    if (rw >= 8 && rh >= 8) {
      cw = rw;
      ch = rh;
    }
  }
  // Borrowed FlyCube Device is Display-thread only. Re-resolve the DXGI
  // popup each shot so a stale HWND does not BitBlt the 2D map tab.
  if (scene) {
    scene->set_gpu_present_visible(true);
    if (HWND live = scene->present_hwnd()) {
      if (IsWindow(live)) {
        hwnd = live;
        ShowWindow(live, SW_SHOWNOACTIVATE);
        SetWindowPos(live, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
      }
    }
    (void)present_shell_scene3d_frame(scene, 900);
    if (HWND live = scene->present_hwnd()) {
      if (IsWindow(live)) {
        hwnd = live;
      }
    }
  } else {
    (void)cam->present_gpu(device, static_cast<uint32_t>(cw),
                           static_cast<uint32_t>(ch));
  }
  if (globe_flythrough) {
    Sleep(100);
  }
  wchar_t path[MAX_PATH] = {};
  if (!exe_capture_path(path, MAX_PATH, bmp_leaf)) {
    return false;
  }
  if (!capture_hwnd_scaled_bmp(hwnd, path, static_cast<int>(kAtmosphereShowcaseW),
                               static_cast<int>(kAtmosphereShowcaseH))) {
    return false;
  }
  int bw = 0;
  int bh = 0;
  BmpFileCheckOpts check;
  check.require_color_diversity = true;
  return bmp_file_has_visible_signal(path, &bw, &bh, check);
}

bool capture_atmosphere_showcase_bmp(AtmosphereShowcaseMode mode,
                                     const char* mode_name,
                                     content::Scene3dPresenter* cam,
                                     render::rhi::Device* device,
                                     HWND present_hwnd,
                                     HWND owned_present_hwnd,
                                     bool want_gpu,
                                     bool globe_flythrough,
                                     ui::views::DrawHost* scene) {
  if (!cam || !device || !mode_name) {
    return !want_gpu;
  }
  wchar_t file[64] = {};
  swprintf_s(file, L"atmosphere-showcase-%S.bmp", mode_name);
  const bool ok = capture_atmosphere_named_bmp(
      cam, device, present_hwnd, owned_present_hwnd, file, globe_flythrough,
      scene);
  if (ok) {
    atmosphere_showcase_mark("bmp-ok");
    wchar_t path[MAX_PATH] = {};
    if (exe_capture_path(path, MAX_PATH, file)) {
      AtmosphereLabelHook hook{mode, cam};
      (void)atmosphere_after_ok(path, static_cast<int>(kAtmosphereShowcaseW),
                                static_cast<int>(kAtmosphereShowcaseH), &hook);
    }
    std::fprintf(stderr, "atmosphere-showcase: wrote scaled %ux%u %S\n",
                 kAtmosphereShowcaseW, kAtmosphereShowcaseH, mode_name);
    return true;
  }
  if (want_gpu) {
    atmosphere_showcase_mark("bmp-skip");
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
  }
  return !want_gpu;
}

}  // namespace detail
}  // namespace app
