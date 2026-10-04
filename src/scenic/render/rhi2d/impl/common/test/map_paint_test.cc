// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "gdal.h"
#include "gdal_priv.h"
#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "scenic/detail/feature_kind.h"
#include "gis/envelope.h"
#include "gis/map/map.h"
#include "vista/world/terrain/dem/dem_raster.h"
#include "vista/world/terrain/process/dem_hillshade.h"
#include "scenic/render/rhi2d/public/device/renderdevice.h"
#include "scenic/test/paint_test_host.h"
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
  return scenic::detail::find_china_vector_sample();
}

HWND create_paint_test_hwnd(int width, int height) {
  return scenic::detail::create_paint_test_popup(
      L"SmartGisGdiMapPaintTest", L"gdi-map-paint-test", width, height);
}

// Mainland China framing used by matrix ZoomToRect / Views china BMP.
constexpr double kChinaFrameMinX = 80.0;
constexpr double kChinaFrameMinY = 16.0;
constexpr double kChinaFrameMaxX = 128.0;
constexpr double kChinaFrameMaxY = 52.0;
// Soft multiply strength matching Map2dFrameCache hillshade TileSlot.
constexpr float kHillshadeOpacity = 0.72f;

bool is_ocean_clear_rgb(unsigned r, unsigned g, unsigned b) {
  return std::abs(static_cast<int>(r) - 170) <= 8 &&
         std::abs(static_cast<int>(g) - 211) <= 8 &&
         std::abs(static_cast<int>(b) - 223) <= 8;
}

// Top-down BGRA32 → BMP (negative biHeight). Keeps soft-multiply composite.
bool write_bgra32_bmp(const char* path, int width, int height,
                      const std::uint32_t* bits) {
  if (!path || !bits || width <= 0 || height <= 0) {
    return false;
  }
  const DWORD row_bytes = static_cast<DWORD>(width) * 4u;
  const DWORD pixel_bytes = row_bytes * static_cast<DWORD>(height);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;  // 'BM'
  bfh.bfOffBits =
      static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
  bfh.bfSize = bfh.bfOffBits + pixel_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width;
  bih.biHeight = -height;  // top-down
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = pixel_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  const bool ok =
      fwrite(&bfh, sizeof(bfh), 1, f) == 1 &&
      fwrite(&bih, sizeof(bih), 1, f) == 1 &&
      fwrite(bits, 1, static_cast<size_t>(pixel_bytes), f) ==
          static_cast<size_t>(pixel_bytes);
  fclose(f);
  return ok;
}

struct MatrixHillshade {
  bool ok = false;
  std::vector<std::uint8_t> rgba;
  int w = 0;
  int h = 0;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
};

// Process-local bake: matrix cells are short-lived processes; cache still
// avoids a second GDAL open if both MAP and QUICK validate.
const MatrixHillshade& matrix_hillshade_bake() {
  static MatrixHillshade bake;
  static bool attempted = false;
  if (attempted) {
    return bake;
  }
  attempted = true;

  std::string dem_path;
  if (const char* env = std::getenv("SMT_CHINA_DEM");
      env != nullptr && env[0] != '\0') {
    dem_path = env;
  } else {
    dem_path = vista::find_sample_dem_path();
  }
  if (dem_path.empty()) {
    std::fprintf(stderr,
                 "matrix_bmp: hillshade skip - china_dem.tif not found "
                 "(expected ../data/china_dem.tif)\n");
    return bake;
  }

  vista::DemRaster dem;
  if (!dem.load_gdal_raster(dem_path.c_str()) || dem.empty()) {
    std::fprintf(stderr, "matrix_bmp: hillshade skip - DEM load failed (%s)\n",
                 dem_path.c_str());
    return bake;
  }

  vista::HillshadeParams params;
  params.max_edge = 256;
  params.exaggeration = 0.5f;
  if (!vista::shade_dem_rgba(dem, params, &bake.rgba, &bake.w, &bake.h) ||
      bake.w < 2 || bake.h < 2 || bake.rgba.empty()) {
    std::fprintf(stderr, "matrix_bmp: hillshade skip - shade_dem_rgba failed\n");
    bake.rgba.clear();
    bake.w = 0;
    bake.h = 0;
    return bake;
  }
  dem.envelope(&bake.minx, &bake.miny, &bake.maxx, &bake.maxy);
  bake.ok = true;
  std::fprintf(stderr,
               "matrix_bmp: hillshade baked %dx%d from %s opacity=%.2f\n",
               bake.w, bake.h, dem_path.c_str(), kHillshadeOpacity);
  return bake;
}

// Soft-multiply DEM hillshade onto non-ocean BGRA pixels (Views blit math).
void soft_multiply_hillshade_bgra(std::vector<std::uint32_t>* bits, int bmp_w,
                                  int bmp_h, const MatrixHillshade& hs) {
  if (!bits || !hs.ok || bmp_w <= 0 || bmp_h <= 0) {
    return;
  }
  const double dem_dx = hs.maxx - hs.minx;
  const double dem_dy = hs.maxy - hs.miny;
  if (dem_dx <= 0.0 || dem_dy <= 0.0) {
    return;
  }
  const float k = kHillshadeOpacity;
  const int sw = hs.w;
  const int sh = hs.h;
  for (int y = 0; y < bmp_h; ++y) {
    const double lat =
        kChinaFrameMaxY -
        (kChinaFrameMaxY - kChinaFrameMinY) *
            (static_cast<double>(y) /
             static_cast<double>((std::max)(1, bmp_h - 1)));
    for (int x = 0; x < bmp_w; ++x) {
      const size_t pi =
          static_cast<size_t>(y) * static_cast<size_t>(bmp_w) +
          static_cast<size_t>(x);
      const std::uint32_t px = (*bits)[pi];
      const unsigned b = px & 0xffu;
      const unsigned g = (px >> 8) & 0xffu;
      const unsigned r = (px >> 16) & 0xffu;
      if (is_ocean_clear_rgb(r, g, b)) {
        continue;
      }
      const double lon =
          kChinaFrameMinX +
          (kChinaFrameMaxX - kChinaFrameMinX) *
              (static_cast<double>(x) /
               static_cast<double>((std::max)(1, bmp_w - 1)));
      if (lon < hs.minx || lon > hs.maxx || lat < hs.miny || lat > hs.maxy) {
        continue;
      }
      const double u = (lon - hs.minx) / dem_dx;
      const double v = (hs.maxy - lat) / dem_dy;
      int sx = static_cast<int>(u * static_cast<double>(sw - 1) + 0.5);
      int sy = static_cast<int>(v * static_cast<double>(sh - 1) + 0.5);
      sx = (std::max)(0, (std::min)(sw - 1, sx));
      sy = (std::max)(0, (std::min)(sh - 1, sy));
      const size_t so =
          (static_cast<size_t>(sy) * static_cast<size_t>(sw) +
           static_cast<size_t>(sx)) *
          4u;
      if (hs.rgba[so + 3] == 0) {
        continue;
      }
      const float sr = static_cast<float>(hs.rgba[so + 0]) / 255.f;
      const float sg = static_cast<float>(hs.rgba[so + 1]) / 255.f;
      const float sb = static_cast<float>(hs.rgba[so + 2]) / 255.f;
      const float shade = 0.299f * sr + 0.587f * sg + 0.114f * sb;
      const float m = 1.f - k + k * shade;
      const unsigned nb = static_cast<unsigned>(
          (std::min)(255.f, static_cast<float>(b) * m + 0.5f));
      const unsigned ng = static_cast<unsigned>(
          (std::min)(255.f, static_cast<float>(g) * m + 0.5f));
      const unsigned nr = static_cast<unsigned>(
          (std::min)(255.f, static_cast<float>(r) * m + 0.5f));
      (*bits)[pi] = (px & 0xff000000u) | (nr << 16) | (ng << 8) | nb;
    }
  }
}

// Prefer MAP (published front) then QUICK. Skip near-solid ocean clears.
bool save_matrix_bmp(scenic::detail::LPRENDERDEVICE dev, const char* path) {
  if (!dev || !path || path[0] == '\0') {
    return false;
  }
  const scenic::detail::eRDBufferLayer layers[] = {scenic::detail::MRD_BL_MAP,
                                           scenic::detail::MRD_BL_QUICK};
  char tmp[MAX_PATH] = {};
  for (scenic::detail::eRDBufferLayer layer : layers) {
    if (sprintf_s(tmp, "%s.__try_%d.bmp", path, static_cast<int>(layer)) <= 0) {
      continue;
    }
    if (dev->SaveImage(tmp, layer) != SMT_ERR_NONE) {
      DeleteFileA(tmp);
      continue;
    }
    HBITMAP bmp = static_cast<HBITMAP>(LoadImageA(
        nullptr, tmp, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    DeleteFileA(tmp);
    if (!bmp) {
      continue;
    }
    BITMAP bm = {};
    GetObject(bmp, sizeof(bm), &bm);
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
    DeleteObject(bmp);
    if (got <= 0) {
      continue;
    }
    // Reject solid / near-solid ocean clears (kOceanClear ≈ 170,211,223).
    int oceanish = 0;
    int samples = 0;
    for (int i = 0; i < pixels; i += 16) {
      const unsigned b = bits[static_cast<size_t>(i)] & 0xff;
      const unsigned g = (bits[static_cast<size_t>(i)] >> 8) & 0xff;
      const unsigned r = (bits[static_cast<size_t>(i)] >> 16) & 0xff;
      ++samples;
      if (is_ocean_clear_rgb(r, g, b)) {
        ++oceanish;
      }
    }
    const float ocean_frac =
        samples > 0 ? static_cast<float>(oceanish) / static_cast<float>(samples)
                    : 1.f;
    if (ocean_frac > 0.92f) {
      std::fprintf(stderr, "matrix_bmp skip layer=%d ocean_frac=%.2f\n",
                   static_cast<int>(layer), ocean_frac);
      continue;
    }

    // Soft-multiply DEM hillshade onto land, then write composite BMP.
    const MatrixHillshade& hs = matrix_hillshade_bake();
    if (hs.ok) {
      soft_multiply_hillshade_bgra(&bits, bm.bmWidth, bm.bmHeight, hs);
      if (write_bgra32_bmp(path, bm.bmWidth, bm.bmHeight, bits.data())) {
        std::fprintf(stderr,
                     "matrix_bmp=%s layer=%d ocean_frac=%.2f hillshade=1\n",
                     path, static_cast<int>(layer), ocean_frac);
        return true;
      }
      std::fprintf(stderr,
                   "matrix_bmp: composite write failed, falling back to "
                   "SaveImage\n");
    }

    if (dev->SaveImage(path, layer) == SMT_ERR_NONE) {
      std::fprintf(stderr, "matrix_bmp=%s layer=%d ocean_frac=%.2f hillshade=0\n",
                   path, static_cast<int>(layer), ocean_frac);
      return true;
    }
  }
  // Last resort: MAP even if ocean-heavy (keeps a file for the matrix).
  if (dev->SaveImage(path, scenic::detail::MRD_BL_MAP) == SMT_ERR_NONE) {
    std::fprintf(stderr, "matrix_bmp=%s layer=MAP fallback\n", path);
    return true;
  }
  std::fprintf(stderr, "matrix_bmp=%s save FAILED\n", path);
  return false;
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
int count_map_buf_non_white(scenic::detail::LPRENDERDEVICE dev) {
  if (!dev) {
    return 0;
  }
  char dir[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, dir) == 0) {
    return 0;
  }
  // Prefer QUICK (host paint back) then MAP front — SaveImage(MAP) can fail
  // when the shared front HBITMAP is aliased by the worker share_from.
  const scenic::detail::eRDBufferLayer layers[] = {scenic::detail::MRD_BL_QUICK,
                                           scenic::detail::MRD_BL_MAP};
  for (scenic::detail::eRDBufferLayer layer : layers) {
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
  gis::Map map;
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* lyr = ds->GetLayer(li);
    if (!lyr) {
      continue;
    }
    lyr->ResetReading();
    while (OGRFeature* feat = lyr->GetNextFeature()) {
      const gis::FeatureType ft = leftover_feature_type_of(feat);
      if (ft == gis::FtSurface) {
        ++n_region;
      } else if (ft == gis::FtCurve) {
        ++n_line;
      } else if (ft == gis::FtDot) {
        ++n_dot;
      } else if (ft == gis::FtAnno) {
        ++n_anno;
      }
      OGRFeature::DestroyFeature(feat);
    }
    expect(map.AddLayer(lyr), "AddLayer OGR China sample");
  }
  const bool city_pack = path.find("china_city") != std::string::npos;
  // china_city pack: area≈34, line≥1, point≈64; labels folded into point
  // (no FtAnno layer).
  expect(n_region >= (city_pack ? 30 : 8), "several region polygons");
  expect(n_line >= 1, "line features");
  expect(n_dot >= (city_pack ? 50 : 5), "city points");
  expect(n_anno >= (city_pack ? 0 : 5), "annotation text features");
  std::fprintf(stderr, "kinds region=%d line=%d dot=%d anno=%d\n", n_region,
               n_line, n_dot, n_anno);
  expect(map.GetLayerCount() >= 1, "map layer count");
  expect(map.GetOgrLayer(0) != nullptr, "GetOgrLayer");

  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  GDALDataset* donut_ds = nullptr;
  const bool matrix_run = []() {
    const char* p = std::getenv("SMT_RHI2D_MATRIX_BMP");
    return p != nullptr && p[0] != '\0';
  }();
  // Matrix captures want a clean China frame — skip the synthetic donut layer.
  if (!matrix_run) {
    donut_ds =
        mem ? mem->Create("donut_holes", 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
    OGRLayer* donut = add_donut_with_holes(donut_ds);
    expect(donut != nullptr, "in-memory polygon with 5 holes");
    if (donut) {
      expect(map.AddLayer(donut), "AddLayer donut holes");
    }
  }

  // Full China mainland framing (matches Views map2d-showcase china extent).
  constexpr int kW = 1280;
  constexpr int kH = 720;
  HWND hwnd = create_paint_test_hwnd(kW, kH);
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

  const char* port = std::getenv("SMT_RHI2D_PORT");
  if (port == nullptr || port[0] == '\0') {
    port = "gdi";
  }
  char dll_name[64] = {};
#ifdef _DEBUG
  if (_stricmp(port, "gdiplus") == 0 || _stricmp(port, "gdi+") == 0) {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_gdiplus_d.dll");
  } else if (_stricmp(port, "skia") == 0) {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_skia_d.dll");
  } else {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_gdi_d.dll");
  }
#else
  if (_stricmp(port, "gdiplus") == 0 || _stricmp(port, "gdi+") == 0) {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_gdiplus.dll");
  } else if (_stricmp(port, "skia") == 0) {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_skia.dll");
  } else {
    std::snprintf(dll_name, sizeof(dll_name), "scenic_rhi2d_gdi.dll");
  }
#endif
  std::fprintf(stderr, "port=%s dll=%s parallel=%s\n", port, dll_name,
               std::getenv("SMT_RHI2D_PARALLEL")
                   ? std::getenv("SMT_RHI2D_PARALLEL")
                   : "(default tile)");
  HMODULE dll = LoadLibraryA(dll_name);
  expect(dll != nullptr, "LoadLibrary legacy_rhi2d port");
  auto create = dll ? reinterpret_cast<scenic::detail::_CreateRenderDevice>(
                          GetProcAddress(dll, "CreateRenderDevice"))
                    : nullptr;
  auto destroy = dll ? reinterpret_cast<scenic::detail::_DestroyRenderDevice>(
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

  scenic::detail::LPRENDERDEVICE dev = nullptr;
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
  expect(dev->Resize(0, 0, kW, kH) == SMT_ERR_NONE, "Resize");
  std::fprintf(stderr, "step: resize-ok %dx%d\n", kW, kH);
  std::fflush(stderr);
  RenderOptions2d options = {};
  options.bShowMBR = false;
  options.bShowPoint = true;
  options.lPointRaduis = 3;
  dev->SetRenderOptions(options);

  gis::Envelope env;
  map.CalEnvelope();
  map.get_envelope(env);
  expect(env.is_init(), "map envelope");
  base::fRect frt;
  if (city_pack || matrix_run) {
    // Mainland China framing used by --map2d-showcase=china.
    frt.lb.x = 80.f;
    frt.lb.y = 16.f;
    frt.rt.x = 128.f;
    frt.rt.y = 52.f;
  } else {
    frt.lb.x = static_cast<float>(env.MinX);
    frt.lb.y = static_cast<float>(env.MinY);
    frt.rt.x = static_cast<float>(env.MaxX);
    frt.rt.y = static_cast<float>(env.MaxY);
  }
  std::fprintf(stderr, "step: zoom-begin extent=(%.2f,%.2f)-(%.2f,%.2f)\n",
               frt.lb.x, frt.lb.y, frt.rt.x, frt.rt.y);
  std::fflush(stderr);
  expect(dev->ZoomToRect(&map, frt, true) == SMT_ERR_NONE,
         "ZoomToRect realtime");
  std::fprintf(stderr, "step: zoom-ok\n");
  std::fflush(stderr);

  lRect lrt;
  lrt.lb.x = 0;
  lrt.rt.y = 0;
  lrt.rt.x = kW;
  lrt.lb.y = kH;
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
  const int painted = count_map_buf_non_white(dev);
  std::fprintf(stderr, "realtime non-white samples: %d\n", painted);

  if (const char* out_bmp = std::getenv("SMT_RHI2D_MATRIX_BMP");
      out_bmp && out_bmp[0] != '\0') {
    expect(save_matrix_bmp(dev, out_bmp), "matrix SaveImage QUICK|MAP");
  }

  if (!matrix_run) {
    // Wheel-style preview zoom must not crash (virViewport race / blit size).
    std::fprintf(stderr, "step: preview-zoom-begin\n");
    std::fflush(stderr);
    base::lPoint cursor;
    cursor.x = kW / 2;
    cursor.y = kH / 2;
    for (int i = 0; i < 12; ++i) {
      const float fscale = (i % 2 == 0) ? 0.9f : 1.1f;
      expect(dev->PreviewZoomScale(cursor, fscale) == SMT_ERR_NONE,
             "PreviewZoomScale");
      expect(dev->Refresh() == SMT_ERR_NONE, "Refresh after preview zoom");
    }
    expect(dev->ScheduleDelayedRedraw(&map) == SMT_ERR_NONE,
           "ScheduleDelayedRedraw after preview");
    for (int i = 0; i < 300; ++i) {
      (void)dev->Timer();
      ::Sleep(1);
    }
    std::fprintf(stderr, "step: preview-zoom-ok\n");
    std::fflush(stderr);
  } else {
    // Do not Timer() after the BMP: on_timer can finish_interactive_settle
    // and submit another china FrameJob, then destroy races teardown.
    ::Sleep(50);
  }

  if (destroy) {
    destroy(dev);
    dev = nullptr;
  }
  DestroyWindow(hwnd);
  GDALClose(ds);
  if (donut_ds) {
    GDALClose(donut_ds);
  }
  // Matrix cells: Release may detach a still-running FrameJob and leak the
  // device until process exit (release_may_leak). FreeLibrary then unmaps
  // worker code mid-paint → AV on gdiplus/skia. Leave the DLL mapped for
  // matrix runs; the process exits immediately after.
  if (dll && !matrix_run) {
    FreeLibrary(dll);
  }
  return g_fails == 0 ? 0 : 1;
}
