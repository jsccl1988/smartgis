// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "gis/analysis/raster/dem/hillshade.h"
#include "vista/terrain/dem/nv/bake_pixel.h"
#include "vista/terrain/dem/raster/bake_util.h"
#include "vista/terrain/dem/shade/lit_kern.h"
#include "gdal_priv.h"

namespace vista {

bool DemRaster::bake_map_drape_rgba(int max_edge, std::vector<uint8_t>* rgba,
                                    int* out_w, int* out_h) const {
  const std::string path = find_sample_imagery_path();
  if (path.empty()) {
    return false;
  }
  return bake_map_drape_rgba(max_edge, path.c_str(), rgba, out_w, out_h);
}

bool DemRaster::bake_map_drape_rgba(int max_edge, const char* imagery_path,
                                    std::vector<uint8_t>* rgba, int* out_w,
                                    int* out_h) const {
  if (!rgba || empty() || !imagery_path || !imagery_path[0]) {
    return false;
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols_ > max_edge) {
      step_x = cols_ / max_edge;
    }
    if (rows_ > max_edge) {
      step_y = rows_ / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols_ + step_x - 1) / step_x;
  const int h = (rows_ + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }

  GDALAllRegister();
  GDALDataset* ds =
      static_cast<GDALDataset*>(GDALOpen(imagery_path, GA_ReadOnly));
  if (!ds) {
    return false;
  }
  const int n_x = ds->GetRasterXSize();
  const int n_y = ds->GetRasterYSize();
  const int bands = ds->GetRasterCount();
  if (n_x < 2 || n_y < 2 || bands < 1) {
    GDALClose(ds);
    return false;
  }
  double gt[6] = {};
  const bool has_gt = ds->GetGeoTransform(gt) == CE_None &&
                      std::fabs(gt[1]) > 1e-12 && std::fabs(gt[5]) > 1e-12;

  // Cap source plane so RasterIO stays modest; DEM LOD grid samples it.
  int src_w = n_x;
  int src_h = n_y;
  const int src_cap = (std::max)(max_edge, 1024);
  if (src_w > src_cap || src_h > src_cap) {
    const double sx = static_cast<double>(src_cap) / src_w;
    const double sy = static_cast<double>(src_cap) / src_h;
    const double s = (std::min)(sx, sy);
    src_w = (std::max)(2, static_cast<int>(std::lround(n_x * s)));
    src_h = (std::max)(2, static_cast<int>(std::lround(n_y * s)));
  }
  std::vector<uint8_t> plane(
      static_cast<size_t>(src_w) * static_cast<size_t>(src_h) * 4u, 255);
  std::vector<uint8_t> band_buf(
      static_cast<size_t>(src_w) * static_cast<size_t>(src_h), 0);
  auto read_band = [&](int band_1based, int channel) -> bool {
    GDALRasterBand* band = ds->GetRasterBand(band_1based);
    if (!band) {
      return false;
    }
    if (band->RasterIO(GF_Read, 0, 0, n_x, n_y, band_buf.data(), src_w, src_h,
                       GDT_Byte, 0, 0) != CE_None) {
      return false;
    }
    for (size_t i = 0; i < band_buf.size(); ++i) {
      plane[i * 4u + static_cast<size_t>(channel)] = band_buf[i];
    }
    return true;
  };
  bool ok = false;
  if (bands >= 3) {
    ok = read_band(1, 0) && read_band(2, 1) && read_band(3, 2);
  } else {
    ok = read_band(1, 0);
    if (ok) {
      for (size_t i = 0; i < band_buf.size(); ++i) {
        plane[i * 4u + 1] = band_buf[i];
        plane[i * 4u + 2] = band_buf[i];
      }
    }
  }
  GDALClose(ds);
  if (!ok) {
    return false;
  }

  // Imagery geo box (north-up). Fallback: stretch UV across DEM envelope.
  double img_minx = minx_;
  double img_maxx = maxx_;
  double img_miny = miny_;
  double img_maxy = maxy_;
  if (has_gt) {
    const double x0 = gt[0];
    const double y0 = gt[3];
    const double x1 = gt[0] + gt[1] * n_x + gt[2] * n_y;
    const double y1 = gt[3] + gt[4] * n_x + gt[5] * n_y;
    img_minx = (std::min)(x0, x1);
    img_maxx = (std::max)(x0, x1);
    img_miny = (std::min)(y0, y1);
    img_maxy = (std::max)(y0, y1);
  }
  const double img_span_x = img_maxx - img_minx;
  const double img_span_y = img_maxy - img_miny;
  if (!(img_span_x > 1e-9) || !(img_span_y > 1e-9)) {
    return false;
  }

  const float cell_x_deg = static_cast<float>(
      (maxx_ - minx_) / static_cast<double>((std::max)(1, cols_ - 1)));
  const float cell_y_deg = static_cast<float>(
      (maxy_ - miny_) / static_cast<double>((std::max)(1, rows_ - 1)));
  const float cell_x_m = (std::max)(1.f, cell_x_deg * 111320.f);
  const float cell_y_m = (std::max)(1.f, cell_y_deg * 111320.f);
  const float dx_m = 2.f * static_cast<float>(step_x) * cell_x_m;
  const float dy_m = 2.f * static_cast<float>(step_y) * cell_y_m;
  constexpr float kAz = 5.84685f;  // ~335 deg
  constexpr float kSinAlt = 0.5299f;
  constexpr float kCosAlt = 0.8480f;
  constexpr float kExag = 0.45f;

  auto sample_plane = [&](double lon, double lat, uint8_t* out4) {
    float u = static_cast<float>((lon - img_minx) / img_span_x);
    float v = static_cast<float>((img_maxy - lat) / img_span_y);
    u = detail::dem_clampf(u, 0.f, 1.f);
    v = detail::dem_clampf(v, 0.f, 1.f);
    const float fx = u * static_cast<float>(src_w - 1);
    const float fy = v * static_cast<float>(src_h - 1);
    const int c0 = static_cast<int>(std::floor(fx));
    const int r0 = static_cast<int>(std::floor(fy));
    const int c1 = (std::min)(c0 + 1, src_w - 1);
    const int r1 = (std::min)(r0 + 1, src_h - 1);
    const float tx = fx - static_cast<float>(c0);
    const float ty = fy - static_cast<float>(r0);
    for (int ch = 0; ch < 3; ++ch) {
      const float p00 = plane[(static_cast<size_t>(r0) *
                                   static_cast<size_t>(src_w) +
                               static_cast<size_t>(c0)) *
                                  4u +
                              static_cast<size_t>(ch)];
      const float p10 = plane[(static_cast<size_t>(r0) *
                                   static_cast<size_t>(src_w) +
                               static_cast<size_t>(c1)) *
                                  4u +
                              static_cast<size_t>(ch)];
      const float p01 = plane[(static_cast<size_t>(r1) *
                                   static_cast<size_t>(src_w) +
                               static_cast<size_t>(c0)) *
                                  4u +
                              static_cast<size_t>(ch)];
      const float p11 = plane[(static_cast<size_t>(r1) *
                                   static_cast<size_t>(src_w) +
                               static_cast<size_t>(c1)) *
                                  4u +
                              static_cast<size_t>(ch)];
      const float p0 = p00 * (1.f - tx) + p10 * tx;
      const float p1 = p01 * (1.f - tx) + p11 * tx;
      out4[ch] = detail::bake_pack_u8((p0 * (1.f - ty) + p1 * ty) / 255.f);
    }
    out4[3] = 255;
  };

  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  rgba->assign(n * 4u, 0);
  std::vector<float> shade(n, -1.f);
  uint8_t* pixels = rgba->data();
  auto fill_row = [&](int row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    const double lat =
        maxy_ - (static_cast<double>(src_row) + 0.5) / rows_ * (maxy_ - miny_);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      const size_t cell =
          static_cast<size_t>(row) * static_cast<size_t>(w) +
          static_cast<size_t>(col);
      const size_t i = cell * 4u;
      bool land = true;
      if (!land_.empty()) {
        land = land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
      }
      if (!land || meters_at(src_col, src_row) <= 1.f) {
        pixels[i + 0] = 13;
        pixels[i + 1] = 20;
        pixels[i + 2] = 36;
        pixels[i + 3] = 255;
        shade[cell] = -1.f;
        continue;
      }
      const double lon =
          minx_ + (static_cast<double>(src_col) + 0.5) / cols_ *
                      (maxx_ - minx_);
      uint8_t rgb[4] = {};
      sample_plane(lon, lat, rgb);
      pixels[i + 0] = rgb[0];
      pixels[i + 1] = rgb[1];
      pixels[i + 2] = rgb[2];
      pixels[i + 3] = 255;
      const int c0 = (std::max)(0, src_col - step_x);
      const int c1 = (std::min)(cols_ - 1, src_col + step_x);
      const int r0 = (std::max)(0, src_row - step_y);
      const int r1 = (std::min)(rows_ - 1, src_row + step_y);
      shade[cell] = gis::detail::horn_lambert_shade(
          meters_at(c0, src_row), meters_at(c1, src_row), meters_at(src_col, r1),
          meters_at(src_col, r0), dx_m, dy_m, kExag, kAz, kSinAlt, kCosAlt);
    }
  };
  detail::for_each_bake_row(w, h, fill_row);
  // AVX2 / scalar: RGB *= (0.42 + 0.58 * shade); ocean shade < 0 is skipped.
  detail::apply_rgb_lit_mul(pixels, shade.data(), n, 0.42f, 0.58f);
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  return true;
}

}  // namespace vista
