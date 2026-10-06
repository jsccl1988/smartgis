// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/export_frame.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/harness/common/capture/bmp.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/present/rhi_present_session.h"
#include "app/views/harness/common/pump/pump.h"
#include "app/views/runtime/capability/host_paths.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/plugin_host.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

constexpr int kExportW = 640;
constexpr int kExportH = 480;

bool bmp_looks_like_scene3d_content(const wchar_t* path) {
  int bw = 0;
  int bh = 0;
  BmpFileCheckOpts check;
  check.require_color_diversity = true;
  if (!bmp_file_has_visible_signal(path, &bw, &bh, check)) {
    return false;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"rb") != 0 || !f) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER ih{};
  if (std::fread(&fh, sizeof(fh), 1, f) != 1 ||
      std::fread(&ih, sizeof(ih), 1, f) != 1 || fh.bfType != 0x4D42) {
    std::fclose(f);
    return false;
  }
  const int w = ih.biWidth > 0 ? ih.biWidth : -ih.biWidth;
  const int h = ih.biHeight > 0 ? ih.biHeight : -ih.biHeight;
  const int bpp = ih.biBitCount / 8;
  if (w < 8 || h < 8 || (bpp != 3 && bpp != 4)) {
    std::fclose(f);
    return false;
  }
  const int stride = ((w * ih.biBitCount + 31) / 32) * 4;
  std::vector<unsigned char> row(static_cast<size_t>(stride));
  std::fseek(f, static_cast<long>(fh.bfOffBits), SEEK_SET);
  long sum = 0;
  int samples = 0;
  for (int y = 0; y < h; y += (std::max)(1, h / 24)) {
    if (std::fread(row.data(), 1, static_cast<size_t>(stride), f) !=
        static_cast<size_t>(stride)) {
      break;
    }
    for (int x = 0; x < w; x += (std::max)(1, w / 32)) {
      const unsigned char* p = row.data() + static_cast<size_t>(x) * bpp;
      sum += static_cast<int>(p[0]) + p[1] + p[2];
      ++samples;
    }
    if (y + (std::max)(1, h / 24) < h) {
      const long skip =
          static_cast<long>(stride) *
          static_cast<long>((std::max)(1, h / 24) - 1);
      if (skip > 0) {
        std::fseek(f, skip, SEEK_CUR);
      }
    }
  }
  std::fclose(f);
  if (samples < 8) {
    return false;
  }
  // Reject solid near-white flip-model BitBlt (mean channel ~243).
  const double mean =
      static_cast<double>(sum) / (static_cast<double>(samples) * 3.0);
  return mean < 220.0 && mean > 8.0;
}

bool write_hwnd_scaled_bmp(HWND hwnd,
                           const wchar_t* filename,
                           int dst_w,
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
  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());
  FILE* out = nullptr;
  if (_wfopen_s(&out, filename, L"wb") != 0 || !out) {
    return false;
  }
  const bool ok = std::fwrite(&fh, sizeof(fh), 1, out) == 1 &&
                  std::fwrite(&bi, sizeof(bi), 1, out) == 1 &&
                  std::fwrite(pixels.data(), 1, pixels.size(), out) ==
                      pixels.size();
  std::fclose(out);
  return ok;
}

bool write_engine_sidecar(const wchar_t* bmp_w, const char* engine) {
  if (!bmp_w || !engine) {
    return false;
  }
  std::wstring path(bmp_w);
  const size_t dot = path.find_last_of(L'.');
  if (dot != std::wstring::npos) {
    path.resize(dot);
  }
  path += L".engine.txt";
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(engine, 1, std::strlen(engine), f) ==
                      std::strlen(engine) &&
                  std::fputc('\n', f) != EOF;
  std::fclose(f);
  return ok;
}

bool frame_for_export(Browser& browser, const std::string& frame) {
  content::ViewFrame* vf = browser.view_frame();
  if (!vf) {
    return false;
  }
  if (frame == "china_product") {
    ensure_china_maplibre_carto(browser);
    frame_china_map2d(browser, kExportW, kExportH);
  } else if (frame == "unit_square") {
    constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
    vf->apply_world_extent(kUnit, kExportW, kExportH);
  } else if (frame == "document_extent") {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // compute_extent returns map space (y = -lat). apply_world_extent expects
      // lon/lat Extent2 and converts to map internally — convert here once.
      const double lat_min = -maxy;
      const double lat_max = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (lat_max - lat_min) * 0.15);
      const content::Extent2 live{minx - pad_x, lat_min - pad_y, maxx + pad_x,
                                  lat_max + pad_y};
      vf->apply_world_extent(live, kExportW, kExportH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kExportW, kExportH);
    }
  } else {
    double min_lon = 0.0;
    double min_lat = 0.0;
    double max_lon = 0.0;
    double max_lat = 0.0;
    PluginShell* shell = browser.plugins();
    content::PluginHost* host = shell ? shell->host() : nullptr;
    if (!host || !host->lookup_export_frame(frame, &min_lon, &min_lat, &max_lon,
                                            &max_lat)) {
      return false;
    }
    const content::Extent2 extent{min_lon, min_lat, max_lon, max_lat};
    vf->apply_world_extent(extent, kExportW, kExportH);
  }
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

bool export_scene3d_bmp(Browser* b, const wchar_t* bmp_w) {
  content::Scene3dPresenter* cam = b->scene3d();
  if (!cam) {
    return false;
  }
  // Prove FlyCube GpuPresent is live, then soft-export the shared DEM mesh.
  // DXGI BitBlt of flip-model HWND is often solid black/white and is not
  // the score SoT (browse.3d uses the same software hypsometric path).
  apply_china_scene3d_orbit(*b);
  if (HWND shell = b->hwnd()) {
    if (IsWindow(shell)) {
      SetWindowPos(shell, nullptr, 0, 0, 1280, 800,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
      pump_messages(80);
    }
  }
  b->select_map_tab(1);
  pump_messages(120);
  ui::views::DrawHost* scene = b->scene_draw_host();
  bool flycube_live = false;
  if (scene && content::prefer_scene3d_flycube()) {
    scene->set_gpu_present_visible(true);
    if (scene->attach_mode() != ui::views::DrawHost::AttachMode::kGpuPresent) {
      b->select_map_tab(1);
      pump_messages(200);
    }
    int present_w = 0;
    int present_h = 0;
    if (HWND ph = scene->present_hwnd()) {
      if (IsWindow(ph)) {
        RECT prc = {};
        GetClientRect(ph, &prc);
        present_w = prc.right - prc.left;
        present_h = prc.bottom - prc.top;
      }
    }
    if (scene->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent) {
      flycube_live = present_shell_scene3d_frame(scene, 1200);
      if (!flycube_live) {
        flycube_live = scene->last_gpu_present_ok();
      }
    }
    if (flycube_live && present_w >= 640 && present_h >= 360) {
      HWND present = scene->present_hwnd();
      if (present && IsWindow(present)) {
        ShowWindow(present, SW_SHOWNOACTIVATE);
        SetWindowPos(present, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        pump_messages(40);
        (void)present_shell_scene3d_frame(scene, 400);
        if (write_hwnd_scaled_bmp(present, bmp_w, kExportW, kExportH) &&
            bmp_looks_like_scene3d_content(bmp_w)) {
          (void)write_engine_sidecar(bmp_w, "FlyCube/DX12");
          write_mark(kUiShowcaseMarkLeaf, "interact-3d-gpu-ok", false);
          return true;
        }
        DeleteFileW(bmp_w);
      }
    }
  }
  // Soft paint must not race the Display thread: holding present_mu_ while
  // FlyCube is mid-present has aborted (exit 3) after interact-3d-gestures.
  if (scene) {
    scene->pause_present();
    pump_messages(60);
  }
  if (content::OrbitFrame* orbit = b->orbit_frame()) {
    orbit->set_distance(2.35f);
    orbit->set_pitch(0.62f);
  }
  constexpr int kW = kExportW;
  constexpr int kH = kExportH;
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kW;
  bmi.bmiHeader.biHeight = -kH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
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
  cam->gpu().set_wireframe_enabled(false);
  cam->clear_overlay_tin_mesh();
  if (flycube_live) {
    cam->set_render_engine_name("FlyCube/DX12");
  }
  bool painted = false;
  try {
    cam->software().paint(mem, kW, kH, true);
    painted = true;
  } catch (...) {
    painted = false;
  }
  SelectObject(mem, old);
  bool wrote = false;
  if (painted) {
    const DWORD image_bytes = static_cast<DWORD>(kW * 4) * static_cast<DWORD>(kH);
    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + image_bytes;
    BITMAPINFOHEADER bih = bmi.bmiHeader;
    bih.biSizeImage = image_bytes;
    FILE* f = nullptr;
    if (_wfopen_s(&f, bmp_w, L"wb") == 0 && f) {
      wrote = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
              std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
              std::fwrite(bits, 1, image_bytes, f) == image_bytes;
      std::fclose(f);
    }
  }
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  if (wrote) {
    (void)write_engine_sidecar(bmp_w, flycube_live ? "FlyCube/DX12" : "GDI");
    if (flycube_live) {
      write_mark(kUiShowcaseMarkLeaf, "interact-3d-gpu-ok", false);
    }
  }
  return wrote;
}

bool export_map2d_bmp(Browser* b, const wchar_t* bmp_w, const std::string& frame) {
  content::Map2dPresenter* map2d = b->map2d();
  if (!map2d) {
    return false;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    return false;
  }
  if (!frame_for_export(*b, frame.empty() ? "document_extent" : frame)) {
    return false;
  }
  content::MapScene* doc = b->document();
  if (!doc || doc->feature_count() == 0) {
    return false;
  }
  map2d->bind(doc, b->view_frame());
  map2d->invalidate_frame_cache();
  if (ui::views::DrawHost* pane = b->draw_host()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
      UpdateWindow(pane->native_view());
    }
    pane->invalidate_native();
    pane->sync_identity_frame();
  }
  pump_messages(200);
  return map2d->export_bmp(bmp_a, kExportW, kExportH);
}

bool export_bmp_leaf(Browser& browser,
                     const std::string& leaf,
                     const std::string& frame) {
  if (leaf.empty()) {
    return false;
  }
  const std::wstring leaf_w = host_utf8_to_wide(leaf);
  if (leaf_w.empty()) {
    return false;
  }
  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
    return false;
  }
  if (frame == "scene3d") {
    return export_scene3d_bmp(&browser, bmp_w);
  }
  return export_map2d_bmp(&browser, bmp_w, frame);
}

}  // namespace

void bind_export(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  out->export_bmp = [b](const std::string& leaf, const std::string& frame) {
    return export_bmp_leaf(*b, leaf, frame);
  };
}

}  // namespace detail
}  // namespace app
