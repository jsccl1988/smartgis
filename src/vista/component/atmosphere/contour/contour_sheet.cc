// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/atmosphere/contour/contour_sheet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "vista/terrain/dem/dem_contour.h"

namespace vista {
namespace atmosphere {
namespace {

constexpr float kNoSkip = -1.e30f;

bool field_value_range(const float* field, int cols, int rows, float* out_min,
                       float* out_max) {
  if (!field || !out_min || !out_max || cols < 1 || rows < 1) {
    return false;
  }
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  float fmin = std::numeric_limits<float>::infinity();
  float fmax = -std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < n; ++i) {
    const float v = field[i];
    if (!std::isfinite(v)) {
      continue;
    }
    fmin = (std::min)(fmin, v);
    fmax = (std::max)(fmax, v);
  }
  if (!(fmax > fmin) || !std::isfinite(fmin)) {
    return false;
  }
  *out_min = fmin;
  *out_max = fmax;
  return true;
}

bool compose_display_heights(const float* field, const float* dem_meters,
                             int cols, int rows, float fmin, float fmax,
                             const ContourSheetOptions& opts,
                             std::vector<float>* out) {
  if (!field || !out || cols < 2 || rows < 2) {
    return false;
  }
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  const float fspan = fmax - fmin;
  out->assign(n, 0.f);
  for (std::size_t i = 0; i < n; ++i) {
    float v = field[i];
    if (!std::isfinite(v)) {
      v = fmin;
    }
    const float undulation = ((v - fmin) / fspan) * opts.value_to_meters;
    float dem_y = 0.f;
    if (dem_meters) {
      const float dm = dem_meters[i];
      if (std::isfinite(dm) && dm > 1.f) {
        dem_y = dm * opts.dem_vert_exag;
      }
    }
    (*out)[i] = dem_y + opts.dem_offset_m + undulation;
  }
  return true;
}

}  // namespace

const char* contour_channel_title(FieldChannel channel) {
  switch (channel) {
    case FieldChannel::kWindU:
      return "Wind U";
    case FieldChannel::kWindV:
      return "Wind V";
    case FieldChannel::kWaveHs:
      return "Wave Hs";
    case FieldChannel::kWaveDir:
      return "Wave Dir";
    case FieldChannel::kCloudCover:
      return "Cloud Cover";
    case FieldChannel::kCloudBase:
      return "Cloud Base";
    case FieldChannel::kCloudTop:
      return "Cloud Top";
    case FieldChannel::kSeaMask:
      return "Sea Mask";
    default:
      return "Field";
  }
}

ContourSheet::ContourSheet() = default;
ContourSheet::~ContourSheet() = default;

void ContourSheet::clear() {
  surface_.xyz.clear();
  surface_.indices.clear();
  surface_.rgba.clear();
  surface_.uvs.clear();
  curve_xyz_.clear();
  color_scale_ = ContourColorScale{};
  field_value_min_ = 0.f;
  field_value_max_ = 1.f;
}

bool ContourSheet::rebuild(const float* field, const float* dem_meters, int cols,
                           int rows, double min_lon, double min_lat,
                           double max_lon, double max_lat,
                           const ContourSheetOptions& opts) {
  clear();
  if (!field || cols < 2 || rows < 2 || !(max_lon > min_lon) ||
      !(max_lat > min_lat) ||
      (!opts.curves && !opts.surface && !opts.color_scale)) {
    return false;
  }
  float fmin = 0.f;
  float fmax = 1.f;
  if (!field_value_range(field, cols, rows, &fmin, &fmax)) {
    fmin = 0.f;
    fmax = 1.f;
  }
  field_value_min_ = fmin;
  field_value_max_ = fmax;

  bool ok = false;
  if (opts.curves || opts.surface) {
    std::vector<float> display;
    if (!compose_display_heights(field, dem_meters, cols, rows, fmin, fmax, opts,
                                 &display)) {
      return false;
    }
    // Display heights already include dem_offset; pass z_offset=0 so contour
    // isolevels and TIN verts stay on the same composed sheet.
    if (opts.surface) {
      ok = build_contour_surface_tin(
              display.data(), cols, rows, min_lon, min_lat, max_lon, max_lat,
              /*z_offset=*/0.f, kNoSkip, opts.surface_alpha, &surface_.xyz,
              &surface_.indices, &surface_.rgba, &surface_.uvs) ||
          ok;
    }
    if (opts.curves) {
      ok = extract_contour_curves_3d(display.data(), cols, rows, min_lon,
                                     min_lat, max_lon, max_lat, opts.interval_m,
                                     /*z_offset=*/0.f, kNoSkip, &curve_xyz_) ||
           ok;
    }
  }

  if (opts.color_scale) {
    const char* title =
        opts.scale_title.empty() ? "Contour" : opts.scale_title.c_str();
    ok = build_contour_color_scale(fmin, fmax, title, opts.scale_layout,
                                   &color_scale_) ||
         ok;
  }
  return ok;
}

bool ContourSheet::rebuild_from_store(const FieldStore& store,
                                      FieldChannel channel,
                                      const FieldGrid& grid, double time_sec,
                                      const float* dem_meters,
                                      const ContourSheetOptions& opts) {
  if (grid.empty() || grid.cols < 2 || grid.rows < 2) {
    clear();
    return false;
  }
  const std::size_t n = grid.cell_count();
  std::vector<float> field(n, 0.f);
  const double dlon =
      (grid.max_lon - grid.min_lon) /
      static_cast<double>((std::max)(1, grid.cols - 1));
  const double dlat =
      (grid.max_lat - grid.min_lat) /
      static_cast<double>((std::max)(1, grid.rows - 1));
  for (int row = 0; row < grid.rows; ++row) {
    // Row 0 = north / max-lat (matches dem_contour / DEM bake).
    const double lat = grid.max_lat - static_cast<double>(row) * dlat;
    for (int col = 0; col < grid.cols; ++col) {
      const double lon = grid.min_lon + static_cast<double>(col) * dlon;
      field[static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
            static_cast<std::size_t>(col)] =
          store.sample(channel, lon, lat, time_sec);
    }
  }
  ContourSheetOptions local = opts;
  if (local.scale_title.empty()) {
    local.scale_title = contour_channel_title(channel);
  }
  return rebuild(field.data(), dem_meters, grid.cols, grid.rows, grid.min_lon,
                 grid.min_lat, grid.max_lon, grid.max_lat, local);
}

}  // namespace atmosphere
}  // namespace vista
