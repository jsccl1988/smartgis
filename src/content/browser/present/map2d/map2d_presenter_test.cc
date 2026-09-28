// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"

#include <cstdio>
#include <memory>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
  // Views 2D RHI path: MapScene 鈫?map2d Layout/Pass on Null device.
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
        "china_city.gpkg",
        "china_city.geojson",
        "out\\china_city.gpkg",
        "out\\china_city.geojson",
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
  {
    content::MapScene scene;
    scene.seed_default();
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

    expect(presenter.present_gpu(device.get(), 128, 128), "static second present");
    expect(presenter.layout_build_count() == 1,
           "static repeat does not rebuild layout");
    expect(presenter.last_present_reused_layout(),
           "static repeat reuses layout");

    frame.apply_pan(16, -8);
    expect(presenter.present_gpu(device.get(), 128, 128), "interactive pan present");
    expect(presenter.layout_build_count() == 1,
           "pan within zoom bucket skips layout");
    expect(presenter.last_present_reused_layout(),
           "pan reuses cached MapFrame");

    // Same camera again 鈫?settle rebuild for GPU labels.
    expect(presenter.present_gpu(device.get(), 128, 128), "settle present");
    expect(presenter.layout_build_count() == 2, "settle rebuilds layout once");
    expect(!presenter.last_present_reused_layout(),
           "settle is a full rebuild");

    presenter.invalidate_frame_cache();
    expect(presenter.present_gpu(device.get(), 128, 128), "after invalidate");
    expect(presenter.layout_build_count() == 3,
           "invalidate forces a new layout build");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d map2d_presenter_test fail(s)\n", g_fails);
  }
  return g_fails;
}
