// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gdal.h"
#include "gdal_priv.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/camera.h"
#include "legacy/render/scene3d/bl3d_scene.h"
#include "legacy/render/scene3d/map_to_scene.h"
#include "ogrsf_frmts.h"

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

std::string exe_dir() {
  char path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i + 1] = '\0';
      break;
    }
  }
  return path;
}

std::string find_china_plp() {
  const std::string dir = exe_dir();
  const char* rel[] = {
      "china_city.gpkg",
      "china_city.geojson",
      "china_plp.geojson",
      "testing\\data\\china_city.gpkg",
      "testing\\data\\china_city.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_city.gpkg",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_city.gpkg",
      "..\\..\\testing\\data\\china_plp.geojson",
  };
  for (const char* r : rel) {
    const std::string cand = dir + r;
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

int count_non_black_hwnd(HWND hwnd, int w, int h) {
  HDC hdc = GetDC(hwnd);
  if (!hdc) {
    return 0;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC mem = CreateCompatibleDC(hdc);
  HBITMAP bmp = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !bmp || !bits) {
    if (bmp) {
      DeleteObject(bmp);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(hwnd, hdc);
    return 0;
  }
  HGDIOBJ old = SelectObject(mem, bmp);
  BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
  const auto* px = static_cast<const std::uint32_t*>(bits);
  int n = 0;
  for (int i = 0; i < w * h; i += 4) {
    const unsigned r = px[i] & 0xff;
    const unsigned g = (px[i] >> 8) & 0xff;
    const unsigned b = (px[i] >> 16) & 0xff;
    if (r > 8 || g > 8 || b > 8) {
      ++n;
    }
  }
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, hdc);
  return n;
}

}  // namespace

int main() {
  GDALAllRegister();
  const std::string path = find_china_plp();
  expect(!path.empty(), "china_plp.geojson next to exe or testing/data");
  if (path.empty()) {
    return 1;
  }

  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx(path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
                 nullptr, nullptr));
  expect(ds != nullptr, "GDALOpenEx china_plp");
  expect(ds && ds->GetLayerCount() > 0, "china_plp has a layer");
  if (!ds || ds->GetLayerCount() < 1) {
    if (ds) {
      GDALClose(ds);
    }
    return 1;
  }

  HWND hwnd = CreateWindowExW(0, L"STATIC", L"gl-map-paint-test", WS_POPUP, 0,
                              0, 400, 300, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
  expect(hwnd != nullptr, "CreateWindowEx STATIC");
  if (!hwnd) {
    GDALClose(ds);
    return 1;
  }
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);

#ifdef _DEBUG
  HMODULE dll = LoadLibraryA("legacy_render_d.dll");
#else
  HMODULE dll = LoadLibraryA("legacy_render.dll");
#endif
  expect(dll != nullptr, "LoadLibrary legacy_render");
  auto create = dll ? reinterpret_cast<render::_Create3DRenderDevice>(
                          GetProcAddress(dll, "Create3DRenderDevice"))
                    : nullptr;
  auto destroy = dll ? reinterpret_cast<render::_Release3DRenderDevice>(
                           GetProcAddress(dll, "Release3DRenderDevice"))
                     : nullptr;
  expect(create && destroy, "Create/Release3DRenderDevice exports");
  if (!create || !destroy) {
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 1;
  }

  render::LP3DRENDERDEVICE dev = nullptr;
  expect(create(dll, dev) == 0 && dev, "Create3DRenderDevice");
  if (!dev) {
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 1;
  }

  expect(dev->Init(hwnd, "gl-map-paint-test") == SMT_ERR_NONE, "Init");
  std::fprintf(stderr, "step: init-ok\n");
  std::fflush(stderr);

  render::Viewport3D vp = dev->GetViewport();
  render::apply_view3d_viewport(&vp, 400, 300);
  expect(dev->SetViewport(vp) == SMT_ERR_NONE, "SetViewport 400x300");

  render::SmtScene scene;
  scene.Set3DRenderDevice(dev);
  expect(scene.Setup() == SMT_ERR_NONE, "SmtScene::Setup");

  int n_region = 0;
  int n_line = 0;
  int n_dot = 0;
  int n_anno = 0;
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* lyr = ds->GetLayer(li);
    if (!lyr) {
      continue;
    }
    lyr->ResetReading();
    while (OGRFeature* feat = lyr->GetNextFeature()) {
      OGRGeometry* geom = feat->GetGeometryRef();
      const OGRwkbGeometryType gt =
          geom ? wkbFlatten(geom->getGeometryType()) : wkbUnknown;
      const int ai = feat->GetFieldIndex("anno");
      const char* anno = ai >= 0 ? feat->GetFieldAsString(ai) : nullptr;
      if (gt == wkbPolygon || gt == wkbMultiPolygon) {
        ++n_region;
      } else if (gt == wkbLineString || gt == wkbMultiLineString) {
        ++n_line;
      } else if (gt == wkbPoint && anno && anno[0]) {
        ++n_anno;
      } else if (gt == wkbPoint) {
        ++n_dot;
      }
      OGRFeature::DestroyFeature(feat);
    }
  }
  const bool city_pack = path.find("china_city") != std::string::npos;
  expect(n_region >= (city_pack ? 100 : 8), "several region polygons");
  expect(n_line >= 1, "line features");
  expect(n_dot >= (city_pack ? 50 : 5), "city points");
  expect(n_anno >= (city_pack ? 50 : 5), "annotation text features");
  std::fprintf(stderr, "kinds region=%d line=%d dot=%d anno=%d\n", n_region,
               n_line, n_dot, n_anno);

  const int seeded = render::seed_geojson_into_scene(dev, &scene, path.c_str());
  expect(seeded >= (city_pack ? 100 : 20), "seed China sample into 3D scene");
  std::fprintf(stderr, "step: seeded=%d\n", seeded);
  std::fflush(stderr);

  render::SmtPerspCamera camera(dev, vp);
  render::frame_persp_camera_to_aabb(&camera, &vp, scene.GetAabb());
  camera.SetViewport(vp);
  scene.SetSceneCamera(&camera);
  expect(dev->SetViewport(vp) == SMT_ERR_NONE, "SetViewport after frame");

  dev->SetClearColor(render::SmtColor(0.f, 0.f, 0.f, 1.f));
  expect(dev->Clear(CLR_COLOR | CLR_ZBUFFER) == SMT_ERR_NONE, "Clear");
  expect(dev->BeginRender() == SMT_ERR_NONE, "BeginRender");
  expect(camera.Apply() == SMT_ERR_NONE, "camera.Apply");
  {
    render::vSmt3DObjectPtrs objs;
    scene.Get3DObjectPtrs(objs);
    expect(!objs.empty(), "scene has seeded 3D objects");
    for (render::Smt3DObject* obj : objs) {
      if (obj && obj->IsVisible()) {
        obj->Render(dev);
      }
    }
  }
  expect(dev->EndRender() == SMT_ERR_NONE, "EndRender");
  expect(dev->SwapBuffers() == SMT_ERR_NONE, "SwapBuffers");

  const int painted = count_non_black_hwnd(hwnd, 400, 300);
  expect(painted > 20, "3D paint produced non-black pixels");
  std::fprintf(stderr, "3d non-black samples: %d\n", painted);
  std::fflush(stderr);

  if (destroy) {
    destroy(dev);
  }
  DestroyWindow(hwnd);
  GDALClose(ds);
  if (dll) {
    FreeLibrary(dll);
  }
  return g_fails == 0 ? 0 : 1;
}
