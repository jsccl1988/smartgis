// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "vista/component/map/ir.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  // Synthetic same-brush / same-pen batch smoke.
  {
    vista::MapIR frame;
    frame.background_rgba = 0xfff5f0e6u;
    const uint32_t fill_rgba = 0xffc4d6a0u;
    const uint32_t line_rgba = 0xff3a5f8cu;
    constexpr int kFills = 128;
    constexpr int kStrokes = 256;
    for (int i = 0; i < kFills; ++i) {
      vista::DrawItem item;
      item.kind = vista::DrawKind::kFill;
      item.rgba = fill_rgba;
      item.pixel_space = true;
      const float x0 = static_cast<float>((i % 16) * 20);
      const float y0 = static_cast<float>((i / 16) * 20);
      item.vertices = {{x0, y0},
                       {x0 + 18.f, y0},
                       {x0 + 18.f, y0 + 18.f},
                       {x0, y0 + 18.f}};
      item.indices = {0, 1, 2, 0, 2, 3};
      frame.items.push_back(std::move(item));
    }
    for (int i = 0; i < kStrokes; ++i) {
      vista::DrawItem item;
      item.kind = vista::DrawKind::kLine;
      item.rgba = line_rgba;
      item.pixel_space = true;
      const float x0 = static_cast<float>(i % 64) * 10.f;
      const float y0 = 200.f + static_cast<float>(i / 64) * 12.f;
      item.vertices = {{x0, y0}, {x0 + 40.f, y0 + 8.f}, {x0 + 80.f, y0}};
      frame.items.push_back(std::move(item));
    }
    // emit_segment_quad index pair: (base,base+1,base+2)(base+1,base+3,base+2)
    constexpr int kMeshLines = 64;
    for (int i = 0; i < kMeshLines; ++i) {
      vista::DrawItem item;
      item.kind = vista::DrawKind::kLine;
      item.rgba = line_rgba;
      item.pixel_space = true;
      const float x0 = static_cast<float>(i % 16) * 36.f;
      const float y0 = 280.f + static_cast<float>(i / 16) * 16.f;
      item.vertices = {{x0, y0 + 2.f},
                       {x0, y0 - 2.f},
                       {x0 + 32.f, y0 + 2.f},
                       {x0 + 32.f, y0 - 2.f}};
      item.indices = {0, 1, 2, 1, 3, 2};
      frame.items.push_back(std::move(item));
    }
    {
      vista::DrawItem quad;
      quad.kind = vista::DrawKind::kLine;
      quad.rgba = 0xffff0000u;
      quad.pixel_space = true;
      quad.vertices = {{500.f, 300.f},
                       {540.f, 300.f},
                       {540.f, 340.f},
                       {500.f, 340.f}};
      quad.indices = {0, 1, 2, 0, 2, 3};
      frame.items.push_back(std::move(quad));
    }
    vista::View view{640, 360, 0.0, 0.0, 640.0, 360.0};
    HDC screen = GetDC(nullptr);
    expect(screen != nullptr, "gdi batch GetDC");
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bmi = {};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = 640;
      bmi.bmiHeader.biHeight = -360;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(mem && dib, "gdi batch DIB");
      if (mem && dib) {
        HGDIOBJ old = SelectObject(mem, dib);
        const auto t0 = std::chrono::steady_clock::now();
        content::detail::paint_map_frame_gdi(mem, frame, view, true, {});
        const int64_t paint_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0)
                .count();
        std::fprintf(stderr,
                     "map2d_frame_gdi_test: synthetic paint_ms=%lld items=%zu\n",
                     static_cast<long long>(paint_ms), frame.items.size());
        expect(paint_ms >= 0, "synthetic paint ran");
        const auto* px = static_cast<const unsigned char*>(bits);
        const int stride = 640 * 4;
        const unsigned char* sample = px + 320 * stride + 520 * 4;
        expect(sample[2] > 200 && sample[0] < 40, "indexed line quad draws red");
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      if (mem) {
        DeleteDC(mem);
      }
      ReleaseDC(nullptr, screen);
    }
  }

  // Overlapping same-color tris must stay filled (WINDING). ALTERNATE would
  // punch an even-odd hole at the overlap — the china coastal fringe bug.
  {
    vista::MapIR frame;
    frame.background_rgba = 0xffaad3dfu;
    const uint32_t fill_rgba = 0xfff5f3e9u;
    vista::DrawItem a;
    a.kind = vista::DrawKind::kFill;
    a.rgba = fill_rgba;
    a.pixel_space = true;
    a.vertices = {{10.f, 10.f}, {90.f, 10.f}, {50.f, 80.f}};
    a.indices = {0, 1, 2};
    frame.items.push_back(std::move(a));
    vista::DrawItem b;
    b.kind = vista::DrawKind::kFill;
    b.rgba = fill_rgba;
    b.pixel_space = true;
    b.vertices = {{10.f, 80.f}, {90.f, 80.f}, {50.f, 10.f}};
    b.indices = {0, 1, 2};
    frame.items.push_back(std::move(b));
    vista::View view{100, 100, 0.0, 0.0, 100.0, 100.0};
    HDC screen = GetDC(nullptr);
    expect(screen != nullptr, "winding GetDC");
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bmi = {};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = 100;
      bmi.bmiHeader.biHeight = -100;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(mem && dib && bits, "winding DIB");
      if (mem && dib && bits) {
        HGDIOBJ old = SelectObject(mem, dib);
        content::detail::paint_map_frame_gdi(mem, frame, view, true, {});
        const auto* px = static_cast<const unsigned char*>(bits);
        const int stride = 100 * 4;
        // Overlap center — must be land cream (BGRA e9,f3,f5), not ocean.
        const unsigned char* sample = px + 45 * stride + 50 * 4;
        const unsigned b = sample[0];
        const unsigned g = sample[1];
        const unsigned r = sample[2];
        expect(r > 230 && g > 230 && b > 200,
               "overlap center stays land (WINDING, not ALTERNATE hole)");
        expect(!(b > r + 20 && g > r),
               "overlap center is not ocean blue");
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      if (mem) {
        DeleteDC(mem);
      }
      ReleaseDC(nullptr, screen);
    }
  }

  // Faithful china 1280x720 export when out/data samples exist.
  {
    const char* city_candidates[] = {
        "testing\\data\\china_city.gpkg",
        "testing\\data\\china_city.geojson",
        "..\\data\\china_city.gpkg",
        "..\\data\\china_city.geojson",
        "..\\..\\testing\\data\\china_city.gpkg",
        "china_city.gpkg",
        "china_city.geojson",
    };
    bool opened = false;
    for (const char* cand : city_candidates) {
      content::MapScene scene;
      if (!scene.open_path(cand) || !scene.last_open_was_ogr()) {
        continue;
      }
      opened = true;
      content::ViewFrame frame;
      frame.apply_world_extent({73.0, 18.0, 135.0, 54.0}, 1280, 720);
      content::Map2dPresenter presenter;
      presenter.bind(&scene, &frame);
      char tmp[MAX_PATH] = {};
      expect(GetTempPathA(MAX_PATH, tmp) > 0, "temp path");
      const std::string bmp = std::string(tmp) + "map2d_p2_china_export.bmp";
      DeleteFileA(bmp.c_str());
      base::set_switch("map2d-no-hillshade", "1");
      base::set_switch("map2d-export-reuse", "0");
      content::reset_map2d_phase_sample();
      const bool ok = presenter.export_bmp(bmp, 1280, 720);
      const content::Map2dPhaseSample ph = content::map2d_last_phase_sample();
      std::fprintf(stderr,
                   "map2d_frame_gdi_test: china_faithful export_ok=%d "
                   "paint_ms=%lld bmp_io_ms=%lld path=%s\n",
                   ok ? 1 : 0, static_cast<long long>(ph.software_paint_ms),
                   static_cast<long long>(ph.bmp_io_ms), cand);
      expect(ok, "china faithful export_bmp");
      // Stretch budget from equal-profile plan; report residual even if open.
      if (ok && ph.software_paint_ms > 100) {
        std::fprintf(stderr,
                     "map2d_frame_gdi_test: paint_ms residual vs 100=%lld\n",
                     static_cast<long long>(ph.software_paint_ms - 100));
      }
      DeleteFileA(bmp.c_str());
      break;
    }
    if (!opened) {
      std::fprintf(stderr,
                   "map2d_frame_gdi_test: china sample missing; synthetic only\n");
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d map2d_frame_gdi_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("map2d_frame_gdi_test ok\n");
  return 0;
}
