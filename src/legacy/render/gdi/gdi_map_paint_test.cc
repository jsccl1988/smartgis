// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gdal.h"
#include "gdal_priv.h"
#include "legacy/render/bridge/renderdevice.h"
#include "ogrsf_frmts.h"
#include "base/carto/envelope.h"
#include "gis/datasource/gdal/ogr_feature_codec.h"
#include "gis/map/map.h"

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

int count_non_white(HWND hwnd, int w, int h) {
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
    if (r < 250 && g < 250 && b < 250) {
      ++n;
    }
  }
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, hdc);
  return n;
}

OGRLayer* add_donut_with_holes(GDALDataset* ds) {
  if (!ds) {
    return nullptr;
  }
  OGRLayer* lyr = ds->CreateLayer("donut_holes", nullptr, wkbPolygon, nullptr);
  if (!lyr) {
    return nullptr;
  }
  OGRPolygon poly;
  OGRLinearRing outer;
  outer.addPoint(70.0, 15.0);
  outer.addPoint(140.0, 15.0);
  outer.addPoint(140.0, 55.0);
  outer.addPoint(70.0, 55.0);
  outer.closeRings();
  poly.addRing(&outer);
  // Five 4-vertex holes: old DrawPloygon used ring index i as
  // getX(i)/lpPoint[i] and OOB-crashed once i >= hole vertex count.
  for (int h = 0; h < 5; ++h) {
    OGRLinearRing hole;
    const double x = 75.0 + h * 10.0;
    hole.addPoint(x, 20.0);
    hole.addPoint(x + 3.0, 20.0);
    hole.addPoint(x + 3.0, 23.0);
    hole.addPoint(x, 23.0);
    hole.closeRings();
    poly.addRing(&hole);
  }
  OGRFeature* feat = OGRFeature::CreateFeature(lyr->GetLayerDefn());
  if (!feat) {
    return nullptr;
  }
  feat->SetGeometry(&poly);
  const OGRErr err = lyr->CreateFeature(feat);
  OGRFeature::DestroyFeature(feat);
  return err == OGRERR_NONE ? lyr : nullptr;
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

  int n_region = 0;
  int n_line = 0;
  int n_dot = 0;
  int n_anno = 0;
  gis::SmtMap map;
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* lyr = ds->GetLayer(li);
    if (!lyr) {
      continue;
    }
    lyr->ResetReading();
    while (OGRFeature* feat = lyr->GetNextFeature()) {
      const gis::SmtFeatureType ft =
          gis::datasource::infer_feature_type(feat, gis::SmtFtUnknown);
      if (ft == gis::SmtFtSurface) {
        ++n_region;
      } else if (ft == gis::SmtFtCurve) {
        ++n_line;
      } else if (ft == gis::SmtFtDot) {
        ++n_dot;
      } else if (ft == gis::SmtFtAnno) {
        ++n_anno;
      }
      OGRFeature::DestroyFeature(feat);
    }
    expect(map.AddLayer(lyr), "AddLayer OGR China sample");
  }
  const bool city_pack = path.find("china_city") != std::string::npos;
  expect(n_region >= (city_pack ? 100 : 8), "several region polygons");
  expect(n_line >= 1, "line features");
  expect(n_dot >= (city_pack ? 50 : 5), "city points");
  expect(n_anno >= (city_pack ? 50 : 5), "annotation text features");
  std::fprintf(stderr, "kinds region=%d line=%d dot=%d anno=%d\n", n_region,
               n_line, n_dot, n_anno);
  expect(map.GetLayerCount() >= 1, "map layer count");
  expect(map.GetOgrLayer(0) != nullptr, "GetOgrLayer");

  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  GDALDataset* donut_ds =
      mem ? mem->Create("donut_holes", 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
  OGRLayer* donut = add_donut_with_holes(donut_ds);
  expect(donut != nullptr, "in-memory polygon with 5 holes");
  if (donut) {
    expect(map.AddLayer(donut), "AddLayer donut holes");
  }

  HWND hwnd = CreateWindowExW(0, L"STATIC", L"gdi-map-paint-test", WS_POPUP, 0,
                              0, 400, 300, nullptr, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
  expect(hwnd != nullptr, "CreateWindowEx STATIC");
  if (!hwnd) {
    GDALClose(ds);
    if (donut_ds) {
      GDALClose(donut_ds);
    }
    return 1;
  }
  // GetPixel needs a shown window; DWM does not retain bits for hidden HWNDs.
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);

#ifdef _DEBUG
  HMODULE dll = LoadLibraryA("legacy_render_d.dll");
#else
  HMODULE dll = LoadLibraryA("legacy_render.dll");
#endif
  expect(dll != nullptr, "LoadLibrary legacy_render");
  auto create = dll ? reinterpret_cast<render::_CreateRenderDevice>(
                          GetProcAddress(dll, "CreateRenderDevice"))
                    : nullptr;
  auto destroy = dll ? reinterpret_cast<render::_DestroyRenderDevice>(
                           GetProcAddress(dll, "DestroyRenderDevice"))
                     : nullptr;
  expect(create && destroy, "Create/DestroyRenderDevice exports");
  if (!create || !destroy) {
    DestroyWindow(hwnd);
    GDALClose(ds);
    if (donut_ds) {
      GDALClose(donut_ds);
    }
    return 1;
  }

  render::LPRENDERDEVICE dev = nullptr;
  expect(create(dll, dev) == 0 && dev, "CreateRenderDevice");
  if (!dev) {
    DestroyWindow(hwnd);
    GDALClose(ds);
    if (donut_ds) {
      GDALClose(donut_ds);
    }
    return 1;
  }

  expect(dev->Init(hwnd, "gdi-map-paint-test") == SMT_ERR_NONE, "Init");
  std::fprintf(stderr, "step: init-ok\n");
  std::fflush(stderr);
  expect(dev->Resize(0, 0, 400, 300) == SMT_ERR_NONE, "Resize");
  std::fprintf(stderr, "step: resize-ok\n");
  std::fflush(stderr);
  Smt2DRenderPra pra = {};
  pra.bShowMBR = true;
  pra.bShowPoint = true;
  pra.lPointRaduis = 4;
  dev->SetRenderPra(pra);

  base::Envelope env;
  map.CalEnvelope();
  map.get_envelope(env);
  expect(env.is_init(), "map envelope");
  base::fRect frt;
  frt.lb.x = static_cast<float>(env.MinX);
  frt.lb.y = static_cast<float>(env.MinY);
  frt.rt.x = static_cast<float>(env.MaxX);
  frt.rt.y = static_cast<float>(env.MaxY);
  std::fprintf(stderr, "step: zoom-begin\n");
  std::fflush(stderr);
  expect(dev->ZoomToRect(&map, frt, true) == SMT_ERR_NONE,
         "ZoomToRect realtime");
  std::fprintf(stderr, "step: zoom-ok\n");
  std::fflush(stderr);

  lRect lrt;
  lrt.lb.x = 0;
  lrt.rt.y = 0;
  lrt.rt.x = 400;
  lrt.lb.y = 300;
  std::fprintf(stderr, "step: refresh-begin\n");
  std::fflush(stderr);
  expect(dev->RefreshDirectly(&map, lrt, true) == SMT_ERR_NONE,
         "RefreshDirectly realtime");
  std::fprintf(stderr, "step: refresh-ok\n");
  std::fflush(stderr);
  GdiFlush();
  dev->RenderMap();
  GdiFlush();

  const int painted = count_non_white(hwnd, 400, 300);
  expect(painted > 20, "realtime paint produced non-white pixels");
  std::fprintf(stderr, "realtime non-white samples: %d\n", painted);

  // Wheel-style preview zoom must not crash (virViewport race / blit size).
  std::fprintf(stderr, "step: preview-zoom-begin\n");
  std::fflush(stderr);
  base::lPoint cursor;
  cursor.x = 200;
  cursor.y = 150;
  for (int i = 0; i < 12; ++i) {
    const float fscale = (i % 2 == 0) ? 0.9f : 1.1f;
    expect(dev->PreviewZoomScale(cursor, fscale) == SMT_ERR_NONE,
           "PreviewZoomScale");
    expect(dev->Refresh() == SMT_ERR_NONE, "Refresh after preview zoom");
  }
  expect(dev->ScheduleDelayedRedraw(&map) == SMT_ERR_NONE,
         "ScheduleDelayedRedraw after preview");
  std::fprintf(stderr, "step: preview-zoom-ok\n");
  std::fflush(stderr);

  if (destroy) {
    destroy(dev);
  }
  DestroyWindow(hwnd);
  GDALClose(ds);
  if (donut_ds) {
    GDALClose(donut_ds);
  }
  if (dll) {
    FreeLibrary(dll);
  }
  return g_fails == 0 ? 0 : 1;
}
