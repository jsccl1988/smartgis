// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/seed.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "plugin/runtime/host/capability/shell.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "app/views/util/exe_sidecar_path.h"
#include "vista/terrain/dem/dem_raster.h"

#include "cpl_conv.h"
#include "gdal_priv.h"

namespace plugin {
namespace detail {
namespace {

bool path_exists_utf8(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  wchar_t w[MAX_PATH * 2] = {};
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH * 2) <= 0) {
    return false;
  }
  const DWORD attr = GetFileAttributesW(w);
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool sidecar_utf8(const wchar_t* rel, char* out_utf8, size_t out_cap) {
  if (!rel || !out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t out_w[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path(out_w, MAX_PATH, rel)) {
    return false;
  }
  return WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_utf8,
                             static_cast<int>(out_cap), nullptr, nullptr) > 0;
}

// Crop |src| lon/lat window to |dst| GeoTIFF (downsampled to max_dim).
bool crop_raster_window(const char* src, const char* dst, double minx,
                        double miny, double maxx, double maxy, int max_dim,
                        GDALDataType dtype) {
  if (!src || !dst || !(maxx > minx) || !(maxy > miny) || max_dim < 8) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpen(src, GA_ReadOnly)));
  if (!ds || ds->GetRasterCount() < 1) {
    return false;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None || std::abs(gt[1]) < 1e-18 ||
      std::abs(gt[5]) < 1e-18) {
    return false;
  }
  const int src_w = ds->GetRasterXSize();
  const int src_h = ds->GetRasterYSize();
  auto lon_to_col = [&](double lon) {
    return static_cast<int>(std::floor((lon - gt[0]) / gt[1]));
  };
  auto lat_to_row = [&](double lat) {
    return static_cast<int>(std::floor((lat - gt[3]) / gt[5]));
  };
  int c0 = lon_to_col(minx);
  int c1 = lon_to_col(maxx);
  int r0 = lat_to_row(maxy);
  int r1 = lat_to_row(miny);
  if (c1 < c0) {
    std::swap(c0, c1);
  }
  if (r1 < r0) {
    std::swap(r0, r1);
  }
  c0 = (std::max)(0, (std::min)(src_w - 1, c0));
  c1 = (std::max)(c0 + 1, (std::min)(src_w, c1 + 1));
  r0 = (std::max)(0, (std::min)(src_h - 1, r0));
  r1 = (std::max)(r0 + 1, (std::min)(src_h, r1 + 1));
  const int win_w = c1 - c0;
  const int win_h = r1 - r0;
  if (win_w < 4 || win_h < 4) {
    return false;
  }
  int out_w = win_w;
  int out_h = win_h;
  if (out_w > max_dim || out_h > max_dim) {
    const double s =
        static_cast<double>(max_dim) / static_cast<double>((std::max)(out_w, out_h));
    out_w = (std::max)(8, static_cast<int>(std::lround(out_w * s)));
    out_h = (std::max)(8, static_cast<int>(std::lround(out_h * s)));
  }
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(dst);
  const int bands = (std::min)(4, ds->GetRasterCount());
  GDALDatasetUniquePtr out(driver->Create(dst, out_w, out_h, bands, dtype, nullptr));
  if (!out) {
    return false;
  }
  double ogt[6] = {gt[0] + gt[1] * c0 + gt[2] * r0,
                   gt[1] * (static_cast<double>(win_w) / out_w),
                   0.0,
                   gt[3] + gt[4] * c0 + gt[5] * r0,
                   0.0,
                   gt[5] * (static_cast<double>(win_h) / out_h)};
  out->SetGeoTransform(ogt);
  const char* proj = ds->GetProjectionRef();
  if (proj && proj[0]) {
    out->SetProjection(proj);
  }
  for (int b = 1; b <= bands; ++b) {
    GDALRasterBand* sb = ds->GetRasterBand(b);
    GDALRasterBand* ob = out->GetRasterBand(b);
    if (!sb || !ob) {
      return false;
    }
    std::vector<float> buf(static_cast<size_t>(out_w) * static_cast<size_t>(out_h));
    if (sb->RasterIO(GF_Read, c0, r0, win_w, win_h, buf.data(), out_w, out_h,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
      return false;
    }
    if (ob->RasterIO(GF_Write, 0, 0, out_w, out_h, buf.data(), out_w, out_h,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
      return false;
    }
  }
  return true;
}

bool sample_dem_cell_z(const char* dem_path, double lon, double lat, double* z) {
  if (!dem_path || !z) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(
      static_cast<GDALDataset*>(GDALOpen(dem_path, GA_ReadOnly)));
  if (!ds || ds->GetRasterCount() < 1) {
    return false;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    return false;
  }
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  const int col = static_cast<int>(std::floor((lon - gt[0]) / gt[1]));
  const int row = static_cast<int>(std::floor((lat - gt[3]) / gt[5]));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band || col < 0 || row < 0 || col >= width || row >= height) {
    return false;
  }
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  float v = 0.f;
  if (band->RasterIO(GF_Read, col, row, 1, 1, &v, 1, 1, GDT_Float32, 0, 0,
                     nullptr) != CE_None) {
    return false;
  }
  if (!std::isfinite(v)) {
    return false;
  }
  if (nodata_ok && std::abs(static_cast<double>(v) - nodata) < 1e-6) {
    return false;
  }
  *z = static_cast<double>(v);
  return true;
}

// Lowest finite cell in |dem_path| (stride subsample). Flood seed must sit at
// or below tide; a 5x5-min pit below the actual seed pixel made tide < seeds.
bool find_low_dem_seed(const char* dem_path, double* lon, double* lat,
                       double* z) {
  if (!dem_path || !lon || !lat || !z) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(
      static_cast<GDALDataset*>(GDALOpen(dem_path, GA_ReadOnly)));
  if (!ds || ds->GetRasterCount() < 1) {
    return false;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  if (!band || width < 4 || height < 4) {
    return false;
  }
  std::vector<float> elev(static_cast<size_t>(width) * static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, elev.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    return false;
  }
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  double best = std::numeric_limits<double>::infinity();
  int best_c = 0;
  int best_r = 0;
  const int stride = (std::max)(1, (std::min)(width, height) / 96);
  for (int r = 0; r < height; r += stride) {
    for (int c = 0; c < width; c += stride) {
      const float v =
          elev[static_cast<size_t>(r) * static_cast<size_t>(width) +
               static_cast<size_t>(c)];
      if (!std::isfinite(v)) {
        continue;
      }
      if (nodata_ok && std::abs(static_cast<double>(v) - nodata) < 1e-6) {
        continue;
      }
      if (static_cast<double>(v) < best) {
        best = static_cast<double>(v);
        best_c = c;
        best_r = r;
      }
    }
  }
  if (!std::isfinite(best)) {
    return false;
  }
  *lon = gt[0] + gt[1] * (best_c + 0.5) + gt[2] * (best_r + 0.5);
  *lat = gt[3] + gt[4] * (best_c + 0.5) + gt[5] * (best_r + 0.5);
  *z = best;
  return true;
}

// Percentile of finite DEM samples (stride subsample). Used to keep tide
// below the hills so Scene3D still shows hypsometric land beside water TIN.
bool dem_elevation_percentile(const char* dem_path, double frac, double* z) {
  if (!dem_path || !z || frac <= 0.0 || frac >= 1.0) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(
      static_cast<GDALDataset*>(GDALOpen(dem_path, GA_ReadOnly)));
  if (!ds || ds->GetRasterCount() < 1) {
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  if (!band || width < 4 || height < 4) {
    return false;
  }
  std::vector<float> elev(static_cast<size_t>(width) *
                          static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, elev.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    return false;
  }
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  std::vector<float> finite;
  finite.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) / 16u);
  const int stride = (std::max)(1, (std::min)(width, height) / 96);
  for (int r = 0; r < height; r += stride) {
    for (int c = 0; c < width; c += stride) {
      const float v =
          elev[static_cast<size_t>(r) * static_cast<size_t>(width) +
               static_cast<size_t>(c)];
      if (!std::isfinite(v)) {
        continue;
      }
      if (nodata_ok && std::abs(static_cast<double>(v) - nodata) < 1e-6) {
        continue;
      }
      finite.push_back(v);
    }
  }
  if (finite.size() < 8) {
    return false;
  }
  const size_t idx = static_cast<size_t>(frac * static_cast<double>(finite.size() - 1));
  std::nth_element(finite.begin(), finite.begin() + static_cast<std::ptrdiff_t>(idx),
                   finite.end());
  *z = static_cast<double>(finite[idx]);
  return std::isfinite(*z);
}

bool try_crop_china_dem(char* dem_utf8, size_t dem_cap) {
  const std::string src = vista::find_sample_dem_path();
  if (src.empty() || !path_exists_utf8(src.c_str())) {
    return false;
  }
  if (!sidecar_utf8(L"..\\data\\plugin\\stormsurge_china_dem_crop.tif", dem_utf8,
                    dem_cap)) {
    return false;
  }
  if (!crop_raster_window(src.c_str(), dem_utf8, kStormSurgeMinLon,
                          kStormSurgeMinLat, kStormSurgeMaxLon,
                          kStormSurgeMaxLat, 384, GDT_Float32)) {
    return false;
  }
  return path_exists_utf8(dem_utf8);
}

}  // namespace

bool resolve_stormsurge_sample(const wchar_t* leaf, char* out_utf8,
                               size_t out_cap) {
  if (!leaf || !out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t rel0[MAX_PATH] = {};
  wchar_t rel1[MAX_PATH] = {};
  if (wcscpy_s(rel0, L"..\\data\\plugin\\") != 0 || wcscat_s(rel0, leaf) != 0 ||
      wcscpy_s(rel1, L"data\\plugin\\") != 0 || wcscat_s(rel1, leaf) != 0) {
    return false;
  }
  const wchar_t* rels[] = {rel0, rel1};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

bool resolve_stormsurge_inputs(char* dem_utf8, size_t dem_cap,
                               char* coast_utf8, size_t coast_cap) {
  if (!resolve_stormsurge_sample(L"stormsurge_coast_sample.geojson", coast_utf8,
                                 coast_cap)) {
    plugin_showcase_mark("stormsurge-sample-fail");
    return false;
  }
  if (try_crop_china_dem(dem_utf8, dem_cap)) {
    plugin_showcase_mark("china-dem-crop-ok");
    plugin_showcase_mark("sample-ok");
    return true;
  }
  plugin_showcase_mark("china-dem-crop-skip");
  if (!resolve_stormsurge_sample(L"stormsurge_dem_sample.tif", dem_utf8,
                                 dem_cap)) {
    plugin_showcase_mark("stormsurge-sample-fail");
    return false;
  }
  plugin_showcase_mark("sample-ok");
  return true;
}

bool resolve_stormsurge_mask_output(char* out_utf8, size_t out_cap) {
  if (!out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t out_w[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path(out_w, MAX_PATH,
                                    L"..\\data\\plugin\\stormsurge_mask.tif")) {
    plugin_showcase_mark("stormsurge-out-fail");
    return false;
  }
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_utf8,
                          static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
    plugin_showcase_mark("stormsurge-out-fail");
    return false;
  }
  return true;
}

bool load_stormsurge_map_drape(std::vector<uint8_t>* rgba, int* width,
                               int* height) {
  if (!rgba || !width || !height) {
    return false;
  }
  const std::string src = vista::find_sample_imagery_path();
  if (src.empty() || !path_exists_utf8(src.c_str())) {
    return false;
  }
  char crop_utf8[MAX_PATH * 3] = {};
  if (!sidecar_utf8(L"..\\data\\plugin\\stormsurge_map_drape.tif", crop_utf8,
                    sizeof(crop_utf8))) {
    return false;
  }
  if (!crop_raster_window(src.c_str(), crop_utf8, kStormSurgeMinLon,
                          kStormSurgeMinLat, kStormSurgeMaxLon,
                          kStormSurgeMaxLat, 512, GDT_Byte)) {
    return false;
  }
  int tw = 0;
  int th = 0;
  std::vector<uint8_t> tex;
  if (!vista::load_imagery_rgba(crop_utf8, &tex, &tw, &th, 512) || tw < 8 ||
      th < 8 || tex.size() < static_cast<size_t>(tw) * static_cast<size_t>(th) * 4u) {
    return false;
  }
  // Keep DEM hypsometric greens in the mix so landish BMP gates still fire
  // when china_rs is a grey/urban orthophoto.
  for (size_t i = 0; i + 3 < tex.size(); i += 4) {
    const int r = tex[i];
    const int g = tex[i + 1];
    const int b = tex[i + 2];
    tex[i] = static_cast<uint8_t>((r * 5 + 70) / 6);
    tex[i + 1] = static_cast<uint8_t>((g * 5 + 120) / 6);
    tex[i + 2] = static_cast<uint8_t>((b * 5 + 55) / 6);
    tex[i + 3] = 255;
  }
  *rgba = std::move(tex);
  *width = tw;
  *height = th;
  return true;
}

bool load_stormsurge_bmp_rgba(const char* path, std::vector<uint8_t>* rgba,
                              int* width, int* height) {
  if (!path || !rgba || !width || !height) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  BITMAPFILEHEADER fh = {};
  BITMAPINFOHEADER ih = {};
  in.read(reinterpret_cast<char*>(&fh), sizeof(fh));
  in.read(reinterpret_cast<char*>(&ih), sizeof(ih));
  if (!in || fh.bfType != 0x4D42 || ih.biWidth <= 0 || ih.biHeight == 0) {
    return false;
  }
  const int w = ih.biWidth;
  const int h = std::abs(ih.biHeight);
  const int bpp = ih.biBitCount;
  if (bpp != 24 && bpp != 32) {
    return false;
  }
  if (w < 8 || h < 8) {
    return false;
  }
  const int row_bytes = ((w * bpp + 31) / 32) * 4;
  std::vector<uint8_t> raw(static_cast<size_t>(row_bytes) * static_cast<size_t>(h));
  in.seekg(fh.bfOffBits, std::ios::beg);
  in.read(reinterpret_cast<char*>(raw.data()),
          static_cast<std::streamsize>(raw.size()));
  if (!in) {
    return false;
  }
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 255);
  const bool bottom_up = ih.biHeight > 0;
  for (int y = 0; y < h; ++y) {
    const int src_y = bottom_up ? (h - 1 - y) : y;
    const uint8_t* src =
        raw.data() + static_cast<size_t>(src_y) * static_cast<size_t>(row_bytes);
    uint8_t* dst =
        rgba->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
    for (int x = 0; x < w; ++x) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      dst[3] = 255;
      // Keep a land-green bias so hypsometric gates still fire on carto cream.
      dst[0] = static_cast<uint8_t>((dst[0] * 4 + 70) / 5);
      dst[1] = static_cast<uint8_t>((dst[1] * 4 + 130) / 5);
      dst[2] = static_cast<uint8_t>((dst[2] * 4 + 50) / 5);
      src += (bpp == 32) ? 4 : 3;
      dst += 4;
    }
  }
  *width = w;
  *height = h;
  return true;
}

bool seed_stormsurge_processing(HarnessShell& browser, const char* dem_utf8,
                                const char* coast_utf8, const char* out_utf8) {
  if (!browser.plugin_host()) {
    plugin_showcase_mark("plugins-fail");
    return false;
  }

  const std::string dem_esc = json_escape_path(dem_utf8);
  const std::string coast_esc = json_escape_path(coast_utf8);
  const std::string out_esc = json_escape_path(out_utf8);
  const std::string coast_args =
      std::string("{\"coast\":\"") + coast_esc + "\"}";
  if (!browser.plugin_host()->run_processing("stormsurge.load_coast", coast_args)) {
    plugin_showcase_mark("stormsurge-coast-fail");
    return false;
  }

  double seed_lon = kStormSurgeSeedLon;
  double seed_lat = kStormSurgeSeedLat;
  double seed_z = 28.0;
  if (find_low_dem_seed(dem_utf8, &seed_lon, &seed_lat, &seed_z) ||
      sample_dem_cell_z(dem_utf8, seed_lon, seed_lat, &seed_z)) {
    plugin_showcase_mark("dem-seed-z-ok");
  } else {
    plugin_showcase_mark("dem-seed-z-skip");
  }
  // Tide above the valley seed, but below the 70th-percentile DEM so hills
  // stay dry (seed_z+6 drowned the Wuhan china_dem crop in Scene3D).
  double tide = seed_z + 2.5;
  double p70 = seed_z;
  if (dem_elevation_percentile(dem_utf8, 0.70, &p70) && p70 > seed_z + 1.0) {
    const double relief = p70 - seed_z;
    tide = seed_z + std::clamp(relief * 0.40, 1.5, 4.0);
    if (tide >= p70) {
      tide = seed_z + relief * 0.40;
    }
  }
  char tide_buf[64] = {};
  std::snprintf(tide_buf, sizeof(tide_buf), "%.3f", tide);
  const std::string run_args =
      std::string("{\"dem\":\"") + dem_esc + "\",\"output\":\"" + out_esc +
      "\",\"seed_x\":" + std::to_string(seed_lon) +
      ",\"seed_y\":" + std::to_string(seed_lat) +
      ",\"tide_level\":" + tide_buf + ",\"frames\":8}";
  if (!browser.plugin_host()->run_processing("stormsurge.run", run_args)) {
    plugin_showcase_mark("stormsurge-china-run-fail");
    char schematic[MAX_PATH * 3] = {};
    if (!resolve_stormsurge_sample(L"stormsurge_dem_sample.tif", schematic,
                                   sizeof(schematic))) {
      plugin_showcase_mark("stormsurge-run-fail");
      return false;
    }
    const std::string sch_esc = json_escape_path(schematic);
    const std::string fallback =
        std::string("{\"dem\":\"") + sch_esc + "\",\"coast\":\"" + coast_esc +
        "\",\"output\":\"" + out_esc +
        "\",\"seed_x\":114.30,\"seed_y\":30.55,\"tide_level\":58.0,\"frames\":8}";
    if (!browser.plugin_host()->run_processing("stormsurge.run", fallback)) {
      plugin_showcase_mark("stormsurge-run-fail");
      return false;
    }
    plugin_showcase_mark("stormsurge-schematic-fallback");
  }
  plugin_showcase_mark("stormsurge-ok");
  return true;
}

}  // namespace detail
}  // namespace plugin
