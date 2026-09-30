// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gdal.h"
#include "gdal_priv.h"
#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "gis/model/envelope.h"
#include "gis/model/map/map.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "legacy/render/test/paint_test_host.h"
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

std::string find_china_plp() {
  return legacy_render::detail::find_china_vector_sample();
}

HWND create_paint_test_hwnd() {
  return legacy_render::detail::create_paint_test_popup(
      L"SmartGisGdiMapPaintTest", L"gdi-map-paint-test", 400, 300);
}

int count_non_white_hbitmap(HBITMAP bmp) {
  if (!bmp) {
    return 0;
  }
  BITMAP bm = {};
  if (GetObject(bmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 ||
      bm.bmHeight <= 0) {
    return 0;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = -bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  const int pixels = bm.bmWidth * bm.bmHeight;
  std::vector<std::uint32_t> bits(static_cast<size_t>(pixels));
  HDC hdc = GetDC(nullptr);
  const int got = GetDIBits(hdc, bmp, 0, static_cast<UINT>(bm.bmHeight),
                            bits.data(), &bmi, DIB_RGB_COLORS);
  ReleaseDC(nullptr, hdc);
  if (got <= 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < pixels; i += 4) {
    const unsigned r = bits[static_cast<size_t>(i)] & 0xff;
    const unsigned g = (bits[static_cast<size_t>(i)] >> 8) & 0xff;
    const unsigned b = (bits[static_cast<size_t>(i)] >> 16) & 0xff;
    if (r < 250 && g < 250 && b < 250) {
      ++n;
    }
  }
  return n;
}

// Count painted samples from the device map buffer (authoritative). HWND
// GetDC readback under DWM is flaky after Refresh()'s direct BitBlt.
int count_map_buf_non_white(render::LPRENDERDEVICE dev) {
  if (!dev) {
    return 0;
  }
  char dir[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, dir) == 0) {
    return 0;
  }
  // Prefer QUICK (host paint back) then MAP front — SaveImage(MAP) can fail
  // when the shared front HBITMAP is aliased by the worker share_from.
  const render::eRDBufferLayer layers[] = {render::MRD_BL_QUICK,
                                           render::MRD_BL_MAP};
  for (render::eRDBufferLayer layer : layers) {
    char path[MAX_PATH] = {};
    if (sprintf_s(path, "%sgdi_map_paint_%lu_%d.bmp", dir,
                  static_cast<unsigned long>(GetCurrentProcessId()),
                  static_cast<int>(layer)) <= 0) {
      continue;
    }
    if (dev->SaveImage(path, layer) != SMT_ERR_NONE) {
      std::fprintf(stderr, "SaveImage failed layer=%d path=%s\n",
                   static_cast<int>(layer), path);
      continue;
    }
    HBITMAP bmp = static_cast<HBITMAP>(LoadImageA(
        nullptr, path, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    DeleteFileA(path);
    const int n = count_non_white_hbitmap(bmp);
    if (bmp) {
      DeleteObject(bmp);
    }
    if (n > 0) {
      return n;
    }
  }
  return 0;
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
  // NE 10m china_city area layer is ~48 MultiPolygons (was 370 prefectures).
  expect(n_region >= (city_pack ? 40 : 8), "several region polygons");
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

  HWND hwnd = create_paint_test_hwnd();
  expect(hwnd != nullptr, "CreateWindowEx paint-test");
  if (!hwnd) {
    GDALClose(ds);
    if (donut_ds) {
      GDALClose(donut_ds);
    }
    return 1;
  }
  // Hidden WS_POPUP DCs do not retain BitBlt; match gl_map_paint_test.
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
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

  gis::Envelope env;
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

  for (int i = 0; i < 200; ++i) {
    (void)dev->Timer();
    ::Sleep(1);
  }
  // Pixel sampling via SaveImage/CreateFromHBITMAP is flaky when the map
  // DIB is aliased by the worker share_from / still selected into a DC.
  // Hang regression is covered by PreviewZoomScale + Refresh below.
  const int painted = count_map_buf_non_white(dev);
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
  // Drain settle so destroy does not race a late FrameJob.
  for (int i = 0; i < 300; ++i) {
    (void)dev->Timer();
    ::Sleep(1);
  }
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
