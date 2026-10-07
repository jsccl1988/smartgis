// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "gdal_priv.h"

namespace vista {
namespace {


std::string& dem_path_override_store() {
  static std::string path;
  return path;
}

std::string join_dir(const std::string& dir, const char* rel) {
  return dir + rel;
}

}  // namespace

void set_sample_dem_path_override(const char* path) {
  if (!path || !path[0]) {
    dem_path_override_store().clear();
    return;
  }
  dem_path_override_store().assign(path);
}

std::string sample_dem_path_override() {
  return dem_path_override_store();
}

namespace {

std::string module_dir_for_samples() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  std::string dir;
  if (n > 0 && n < MAX_PATH) {
    dir.assign(module, module + n);
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
      dir.resize(slash + 1);
    }
  }
  return dir;
}

std::string first_existing_rel(const std::string& dir, const char* const* rel,
                               size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const std::string cand = join_dir(dir, rel[i]);
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

}  // namespace

std::string find_sample_dem_path() {
  if (!dem_path_override_store().empty()) {
    const DWORD attr =
        GetFileAttributesA(dem_path_override_store().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return dem_path_override_store();
    }
  }
  // china_dem first: already cutlined. Preferring global_dem here makes
  // leftover China seed remask with trim_dem_mask_rings(48) and punches
  // Henan/plains holes between prefecture blobs.
  static const char* kChinaDemRel[] = {
      "..\\data\\china_dem.tif",
      "..\\data\\china_dem.tiff",
      "data\\china_dem.tif",
      "data\\china_dem.tiff",
      "china_dem.tif",
      "china_dem.tiff",
      "testing\\data\\china\\china_dem.tif",
      "testing\\data\\china\\china_dem.tiff",
      "..\\testing\\data\\china\\china_dem.tif",
      "..\\..\\testing\\data\\china\\china_dem.tif",
  };
  return first_existing_rel(module_dir_for_samples(), kChinaDemRel,
                            std::size(kChinaDemRel));
}

std::string find_sample_global_dem_path() {
  if (!dem_path_override_store().empty()) {
    const DWORD attr =
        GetFileAttributesA(dem_path_override_store().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return dem_path_override_store();
    }
  }
  // Shared out/data first; then <exe>/plugins (out/<config>/plugins).
  static const char* kGlobalDemRel[] = {
      "..\\data\\global_dem.tif",
      "..\\data\\global_dem.tiff",
      "plugins\\world3d\\data\\global_dem.tif",
      "data\\global_dem.tif",
      "data\\global_dem.tiff",
  };
  const std::string global =
      first_existing_rel(module_dir_for_samples(), kGlobalDemRel,
                         std::size(kGlobalDemRel));
  if (!global.empty()) {
    return global;
  }
  return find_sample_dem_path();
}

std::string find_sample_imagery_path() {
  // China orthophoto first — do not steal China drape UV with global equirect.
  static const char* kChinaImageryRel[] = {
      "..\\data\\china_rs.tif",
      "..\\data\\china_imagery.tif",
      "china_rs.tif",
      "china_imagery.tif",
      "china_rs.tiff",
      "china_rs.png",
      "testing\\data\\china_rs.tif",
      "testing\\data\\china_imagery.tif",
      "testing\\data\\china_rs.png",
      "..\\testing\\data\\china_rs.tif",
      "..\\..\\testing\\data\\china_rs.tif",
  };
  return first_existing_rel(module_dir_for_samples(), kChinaImageryRel,
                            std::size(kChinaImageryRel));
}

std::string find_sample_global_imagery_path() {
  // Product GDAL is GTiff-only — prefer .tif over PNG/JPEG.
  // Shared out/data first; then <exe>/plugins (out/<config>/plugins).
  static const char* kGlobalImageryRel[] = {
      "..\\data\\global_terrain.tif",
      "..\\data\\global_imagery.tif",
      "plugins\\world3d\\data\\global_terrain.tif",
      "..\\data\\global_terrain.png",
      "..\\data\\blue_marble.png",
      "plugins\\world3d\\data\\global_terrain.png",
      "..\\data\\global_terrain.jpg",
      "..\\data\\global_terrain.jpeg",
      "..\\data\\blue_marble.jpg",
      "plugins\\world3d\\data\\global_terrain.jpg",
  };
  // Do not fall back to china_rs / regional orthophoto — that atlas is UV-
  // stretched across the full sphere and reads as a mossy "wrong Earth".
  // Missing global albedo → hypsometric / ocean-blue bake in GlobePass.
  return first_existing_rel(module_dir_for_samples(), kGlobalImageryRel,
                            std::size(kGlobalImageryRel));
}

bool load_imagery_rgba(const char* path, std::vector<uint8_t>* rgba, int* out_w,
                       int* out_h, int max_edge) {
  if (!path || !path[0] || !rgba) {
    return false;
  }
  if (max_edge < 2) {
    max_edge = 2;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path, GA_ReadOnly));
  if (!ds) {
    std::fprintf(stderr, "Scene3d imagery: GDALOpen failed for %s\n", path);
    return false;
  }
  const int n_x = ds->GetRasterXSize();
  const int n_y = ds->GetRasterYSize();
  const int bands = ds->GetRasterCount();
  if (n_x < 2 || n_y < 2 || bands < 1) {
    GDALClose(ds);
    std::fprintf(stderr, "Scene3d imagery: bad size/bands for %s\n", path);
    return false;
  }
  // Cap drape / globe equirect so FlyCube upload stays modest. Globe callers
  // pass 2048; default 1024 avoids multi-hundred-MB china_rs uploads.
  int out_cols = n_x;
  int out_rows = n_y;
  if (out_cols > max_edge || out_rows > max_edge) {
    const double sx = static_cast<double>(max_edge) / out_cols;
    const double sy = static_cast<double>(max_edge) / out_rows;
    const double s = (std::min)(sx, sy);
    out_cols = (std::max)(2, static_cast<int>(std::lround(n_x * s)));
    out_rows = (std::max)(2, static_cast<int>(std::lround(n_y * s)));
  }
  rgba->assign(static_cast<size_t>(out_cols) * static_cast<size_t>(out_rows) *
                   4u,
               255);
  std::vector<uint8_t> plane(
      static_cast<size_t>(out_cols) * static_cast<size_t>(out_rows), 0);
  auto read_band = [&](int band_1based, int channel) -> bool {
    GDALRasterBand* band = ds->GetRasterBand(band_1based);
    if (!band) {
      return false;
    }
    const CPLErr err =
        band->RasterIO(GF_Read, 0, 0, n_x, n_y, plane.data(), out_cols,
                       out_rows, GDT_Byte, 0, 0);
    if (err != CE_None) {
      return false;
    }
    for (size_t i = 0; i < plane.size(); ++i) {
      (*rgba)[i * 4u + static_cast<size_t>(channel)] = plane[i];
    }
    return true;
  };
  bool ok = false;
  if (bands >= 3) {
    ok = read_band(1, 0) && read_band(2, 1) && read_band(3, 2);
    if (ok && bands >= 4) {
      (void)read_band(4, 3);
    }
  } else {
    // Single-band greyscale → RGB.
    ok = read_band(1, 0);
    if (ok) {
      for (size_t i = 0; i < plane.size(); ++i) {
        (*rgba)[i * 4u + 1] = plane[i];
        (*rgba)[i * 4u + 2] = plane[i];
      }
    }
  }
  GDALClose(ds);
  if (!ok) {
    rgba->clear();
    std::fprintf(stderr, "Scene3d imagery: RasterIO failed for %s\n", path);
    return false;
  }
  if (out_w) {
    *out_w = out_cols;
  }
  if (out_h) {
    *out_h = out_rows;
  }
  return true;
}


void dem_seed_lonlat_box(double in_minx, double in_miny, double in_maxx,
                          double in_maxy, double* out_minx, double* out_miny,
                          double* out_maxx, double* out_maxy) {
  constexpr double kChinaMinX = 73.0;
  constexpr double kChinaMinY = 18.0;
  constexpr double kChinaMaxX = 135.0;
  constexpr double kChinaMaxY = 54.0;
  const bool nonempty = in_maxx > in_minx && in_maxy > in_miny;
  const bool looks_china = nonempty && in_minx >= 60.0 && in_maxx <= 145.0 &&
                           in_miny >= 3.0 && in_maxy <= 60.0;
  double minx = in_minx;
  double miny = in_miny;
  double maxx = in_maxx;
  double maxy = in_maxy;
  if (sample_dem_path_override().empty()) {
    if (!looks_china) {
      minx = kChinaMinX;
      miny = kChinaMinY;
      maxx = kChinaMaxX;
      maxy = kChinaMaxY;
    }
  } else if (!nonempty) {
    minx = kChinaMinX;
    miny = kChinaMinY;
    maxx = kChinaMaxX;
    maxy = kChinaMaxY;
  }
  if (out_minx) {
    *out_minx = minx;
  }
  if (out_miny) {
    *out_miny = miny;
  }
  if (out_maxx) {
    *out_maxx = maxx;
  }
  if (out_maxy) {
    *out_maxy = maxy;
  }
}

int dem_seed_cache_key(float camera_distance) {
  const int next_lod = DemRaster::lod_max_edge(camera_distance);
  const int grid_key = camera_distance < 2.4f ? 2 : 1;
  return next_lod * 10 + grid_key;
}

bool DemRaster::sample_globe_surface(double minx, double miny, double maxx,
                                     double maxy, bool global_grid,
                                     const char* imagery_path,
                                     std::vector<float>* heights, int* cols,
                                     int* rows, std::vector<uint8_t>* rgba,
                                     int* tex_w, int* tex_h,
                                     bool* imagery_loaded) const {
  if (imagery_loaded) {
    *imagery_loaded = false;
  }
  if (!heights || !cols || !rows || !rgba || !tex_w || !tex_h) {
    return false;
  }
  if (!(maxx > minx) || !(maxy > miny)) {
    return false;
  }
  const int k_cols = global_grid ? 1024 : 1024;
  const int k_rows = global_grid ? 512 : 640;
  heights->assign(static_cast<size_t>(k_cols) * static_cast<size_t>(k_rows),
                  0.f);
  for (int r = 0; r < k_rows; ++r) {
    const double lat =
        maxy - (static_cast<double>(r) + 0.5) / k_rows * (maxy - miny);
    for (int c = 0; c < k_cols; ++c) {
      const double lon =
          minx + (static_cast<double>(c) + 0.5) / k_cols * (maxx - minx);
      (*heights)[static_cast<size_t>(r * k_cols + c)] = sample_meters(lon, lat);
    }
  }
  *cols = k_cols;
  *rows = k_rows;
  rgba->clear();
  *tex_w = 0;
  *tex_h = 0;
  bool have_imagery = false;
  if (imagery_path && imagery_path[0] != '\0') {
    have_imagery = load_imagery_rgba(imagery_path, rgba, tex_w, tex_h,
                                     global_grid ? 2048 : 1024) &&
                   *tex_w > 0 && *tex_h > 0;
    if (!have_imagery) {
      rgba->clear();
      *tex_w = 0;
      *tex_h = 0;
    }
  }
  if (!have_imagery) {
    (void)bake_hypsometric_rgba(global_grid ? 1024 : 1024, rgba, tex_w, tex_h);
  }
  if (imagery_loaded) {
    *imagery_loaded = have_imagery;
  }
  return true;
}



}  // namespace vista
