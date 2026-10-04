// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/software/map2d_frame_gdi.h"
#include "gis/tile/provider/tile_provider.h"
#include "vista/map/ir.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

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

int run_map2d_presenter_tests() {
  // P2 software GDI batch smoke: many same-brush fills + same-pen strokes.
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
                     "map2d_presenter_test: gdi_batch_synthetic paint_ms=%lld "
                     "items=%zu\n",
                     static_cast<long long>(paint_ms), frame.items.size());
        expect(paint_ms >= 0, "gdi batch synthetic ran");
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      if (mem) {
        DeleteDC(mem);
      }
      ReleaseDC(nullptr, screen);
    }
  }

  // Faithful china export timing (NO_HILLSHADE / no EXPORT_REUSE) when samples
  // are present under out/data relative to out/Debug cwd.
  {
    const char* city_candidates[] = {
        "..\\data\\china_city.gpkg",
        "..\\data\\china_city.geojson",
        "out\\data\\china_city.gpkg",
        "out\\data\\china_city.geojson",
        "china_city.gpkg",
        "china_city.geojson",
    };
    for (const char* cand : city_candidates) {
      content::MapScene scene;
      if (!scene.open_path(cand) || !scene.last_open_was_ogr()) {
        continue;
      }
      content::ViewFrame frame;
      frame.apply_world_extent({73.0, 18.0, 135.0, 54.0}, 1280, 720);
      content::Map2dPresenter presenter;
      presenter.bind(&scene, &frame);
      char tmp[MAX_PATH] = {};
      if (GetTempPathA(MAX_PATH, tmp) <= 0) {
        break;
      }
      const std::string bmp = std::string(tmp) + "map2d_p2_china_export.bmp";
      DeleteFileA(bmp.c_str());
      base::set_switch("map2d-no-hillshade", "1");
      base::set_switch("map2d-export-reuse", "0");
      content::reset_map2d_phase_sample();
      const bool ok = presenter.export_bmp(bmp, 1280, 720);
      const content::Map2dPhaseSample ph = content::map2d_last_phase_sample();
      std::fprintf(stderr,
                   "map2d_presenter_test: china_faithful export_ok=%d "
                   "paint_ms=%lld bmp_io_ms=%lld path=%s\n",
                   ok ? 1 : 0, static_cast<long long>(ph.software_paint_ms),
                   static_cast<long long>(ph.bmp_io_ms), cand);
      expect(ok, "china faithful export_bmp");
      DeleteFileA(bmp.c_str());
      break;
    }
  }

  // Views 2D RHI path: MapScene �?map2d Layout/Pass on Null device.
  {
    content::MapScene scene;
    scene.seed_default();
    expect(scene.feature_count() > 0, "seed has features for present_gpu");
    content::ViewFrame frame;
    frame.fit_extent(scene, 800, 600);
    content::Map2dPresenter presenter;
    presenter.bind(&scene, &frame);
    std::unique_ptr<render::rhi::Device> device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(device != nullptr && device->initialize(render::rhi::DeviceDesc()),
           "null device for present_gpu");
    expect(presenter.present_gpu(device.get(), 64, 64),
           "present_gpu after seed_default (Null)");
    // kText lives in the map2d frame; success does not need the GDI label overlay.
    expect(presenter.last_gpu_present_ok(),
           "present_gpu success does not depend on GDI label overlay");
    // Selection stroke overlay must not require a live HWND.
    HDC screen = GetDC(nullptr);
    if (screen) {
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bi = {};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = 64;
      bi.bmiHeader.biHeight = -64;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib =
          CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(mem && dib, "annotation DIB");
      if (mem && dib) {
        HGDIOBJ old = SelectObject(mem, dib);
        presenter.paint_annotation_overlay(mem, 64, 64);
        SelectObject(mem, old);
        DeleteObject(dib);
      }
      if (mem) {
        DeleteDC(mem);
      }
      ReleaseDC(nullptr, screen);
    }
  }

  // Mainland framing: ViewFrame::fit_extent on china_city letterboxes 62x36 deg.
  {
    const char* city_candidates[] = {
        "..\\data\\china_city.gpkg",
        "..\\data\\china_city.geojson",
        "out\\data\\china_city.gpkg",
        "out\\data\\china_city.geojson",
        "china_city.gpkg",
        "china_city.geojson",
        "testing\\data\\china_city.gpkg",
        "testing\\data\\china_city.geojson",
    };
    for (const char* cand : city_candidates) {
      content::MapScene scene;
      if (!scene.open_path(cand) || !scene.last_open_was_ogr()) {
        continue;
      }
      content::ViewFrame frame;
      frame.fit_extent(scene, 800, 600);
      const content::Extent2 view = frame.view_world_extent(800, 600);
      expect(view.xmin >= 72.0 && view.xmax <= 136.0,
             "fit lon stays near mainland");
      expect(view.xmax - view.xmin <= 70.0, "fit lon span near mainland width");
      expect(view.ymin < 25.0 && view.ymax > 45.0,
             "fit covers mainland core latitudes");
      break;
    }
  }

  // Basemap underlay count and one-page BMP export.
  {
    content::MapScene scene;
    scene.seed_default();
    content::ViewFrame frame;
    frame.apply_world_extent({73.0, 18.0, 135.0, 54.0}, 256, 256);
    content::Map2dPresenter presenter;
    presenter.bind(&scene, &frame);
    auto provider = std::make_shared<gis::tile::TileProvider>();
    expect(provider->open_xyz("http://tiles.local/{z}/{x}/{y}.png"),
           "basemap open_xyz");
    provider->set_fetch_fn([](const std::string&) {
      net::HttpResult res;
      res.ok = true;
      res.status = 200;
      res.body = "PNG-STUB";
      return res;
    });
    scene.set_basemap_provider(provider);
    expect(scene.has_basemap_provider(), "has basemap");
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 256;
    bmi.bmiHeader.biHeight = -256;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib =
        CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    expect(dib != nullptr, "basemap CreateDIBSection");
    HGDIOBJ old = SelectObject(mem, dib);
    presenter.paint(mem, 256, 256);
    expect(presenter.basemap_tiles_drawn() > 0, "basemap tiles drawn");
    SelectObject(mem, old);
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);

    char tmp[MAX_PATH] = {};
    expect(GetTempPathA(MAX_PATH, tmp) > 0, "export temp");
    std::string bmp = std::string(tmp) + "map_scene_m1_export.bmp";
    DeleteFileA(bmp.c_str());
    expect(presenter.export_bmp(bmp, 320, 240), "export_bmp");
    FILE* bf = nullptr;
    expect(fopen_s(&bf, bmp.c_str(), "rb") == 0 && bf, "open export bmp");
    char magic[2] = {};
    expect(bf && std::fread(magic, 1, 2, bf) == 2, "read BM");
    if (bf) {
      std::fclose(bf);
    }
    expect(magic[0] == 'B' && magic[1] == 'M', "BMP magic");
    DeleteFileA(bmp.c_str());
  }

  // Dual-speed layout cache: static reuse + interactive pan skip rebuild.
  // Real china_city seed (no demo features). Cold layout cost is also gated
  // by the map2d matrix (1280x720); this unit keeps a 128px viewport.
  {
    content::MapScene scene;
    scene.seed_default(/*allow_china_bootstrap=*/true);
    expect(scene.feature_count() > 0, "china seed for layout cache");
    content::ViewFrame frame;
    frame.fit_extent(scene, 128, 128);
    content::Map2dPresenter presenter;
    presenter.bind(&scene, &frame);
    std::unique_ptr<render::rhi::Device> device(
        render::rhi::create_device(render::rhi::Backend::kNull));
    expect(device != nullptr && device->initialize(render::rhi::DeviceDesc()),
           "null device for layout cache");
    expect(presenter.present_gpu(device.get(), 128, 128), "cache first present");
    expect(presenter.layout_build_count() == 1, "first present builds layout");
    expect(!presenter.last_present_reused_layout(),
           "first present is a full build");
    {
      const content::Map2dPhaseSample phase =
          content::map2d_last_phase_sample();
      std::fprintf(stderr,
                   "map2d_presenter_test: cold layout_ms=%lld (china seed)\n",
                   static_cast<long long>(phase.layout_ms));
      std::fflush(stderr);
      // Real china_city cold layout is matrix-gated (map2d equal-profile);
      // this unit only proves cache reuse, not absolute layout_ms.
      expect(phase.layout_ms >= 0, "cold layout_ms recorded");
    }

    expect(presenter.present_gpu(device.get(), 128, 128), "static second present");
    expect(presenter.layout_build_count() == 1,
           "static repeat does not rebuild layout");
    expect(presenter.last_present_reused_layout(),
           "static repeat reuses layout");
    {
      // StaticReuse must not re-note a rebuild; last sample stays from cold.
      expect(presenter.layout_build_count() == 1, "StaticReuse keeps build count");
    }

    frame.apply_pan(16, -8);
    expect(presenter.present_gpu(device.get(), 128, 128), "interactive pan present");
    expect(presenter.layout_build_count() == 1,
           "pan within zoom bucket skips layout");
    expect(presenter.last_present_reused_layout(),
           "pan reuses cached MapIR");

    // Same camera again after quiet settle debounce (~200ms) �?settle rebuild
    // for GPU labels.
    Sleep(250);
    expect(presenter.present_gpu(device.get(), 128, 128), "settle present");
    expect(presenter.layout_build_count() == 2, "settle rebuilds layout once");
    expect(!presenter.last_present_reused_layout(),
           "settle is a full rebuild");

    presenter.invalidate_frame_cache();
    expect(presenter.present_gpu(device.get(), 128, 128), "after invalidate");
    expect(presenter.layout_build_count() == 3,
           "invalidate forces a new layout build");
  }

  {
    base::set_switch("map2d-engine", "");
    content::Map2dPresenter off;
    expect(!off.hosts_scenic_present(), "default map2d not scenic");
    base::set_switch("map2d-engine", "scenic");
    content::Map2dPresenter on;
    expect(content::prefer_map2d_scenic(), "env scenic map2d");
    expect(on.hosts_scenic_present(),
           "map2d env scenic hosts scenic.dll");
    base::set_switch("map2d-engine", "");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d map2d_presenter_test fail(s)\n", g_fails);
  }
  return g_fails;
}
