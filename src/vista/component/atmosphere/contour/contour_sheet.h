// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_ATMOSPHERE_CONTOUR_CONTOUR_SHEET_H_
#define VISTA_COMPONENT_ATMOSPHERE_CONTOUR_CONTOUR_SHEET_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vista/component/atmosphere/contour/contour_color_scale.h"
#include "vista/component/atmosphere/field/field_channel.h"
#include "vista/component/atmosphere/field/field_store.h"
#include "vista/vista_export.h"

namespace vista {
namespace atmosphere {

// Options for Origin-style stacked contour sheet above DEM.
struct ContourSheetOptions {
  bool curves = true;
  bool surface = true;
  // Screen-ortho jet color scale on the viewport side (bar + tick text).
  bool color_scale = true;
  // 0 = auto from pick_contour_interval_m on the composed display heights.
  float interval_m = 0.f;
  // Constant lift (meters, already in display/mesh Y units) above DEM base so
  // the sheet does not collide with terrain when presenting.
  float dem_offset_m = 500.f;
  // Maps field value delta into additional display elevation undulation.
  float value_to_meters = 1.f;
  // Applied to optional DEM meters before adding dem_offset_m. Default is
  // milder than 1.0 so draped sheets match DEM fit_vertical_exaggeration.
  float dem_vert_exag = 0.35f;
  // Per-vertex alpha for the jet TIN (0..1).
  float surface_alpha = 0.55f;
  ContourScaleLayout scale_layout;
  // Optional HUD title; empty keeps a default from the field channel when
  // rebuilding from FieldStore.
  std::string scale_title;
};

// GPU-ready IR for one stacked field sheet: jet TIN + true-3D isoline segments.
struct ContourSheetMesh {
  // Leftover Y-up: X=-lon, Y=elev, Z=lat.
  std::vector<float> xyz;
  std::vector<uint32_t> indices;
  // 4 floats per vertex (RGBA 0..1); empty when surface disabled.
  std::vector<float> rgba;
  // 2 floats per vertex for overlay drapes (matches xyz vert count).
  std::vector<float> uvs;

  bool empty() const { return xyz.empty() || indices.empty(); }
};

// Builds contour curves and/or a TIN surface from a regular field grid, lifted
// above optional DEM heights for stacked presentation (CPU IR only; no RHI).
// Optionally emits a screen-orthographic side color scale (ramp + text anchors).
class VISTA_EXPORT ContourSheet {
 public:
  ContourSheet();
  ~ContourSheet();

  ContourSheet(const ContourSheet&) = delete;
  ContourSheet& operator=(const ContourSheet&) = delete;

  // Compose display heights = dem*exag + dem_offset + field*scale, then emit
  // curves and/or TIN. |dem_meters| may be null (flat base at dem_offset_m).
  // |field| / |dem_meters| are row-major; row 0 = north / max-lat.
  bool rebuild(const float* field, const float* dem_meters, int cols, int rows,
               double min_lon, double min_lat, double max_lon, double max_lat,
               const ContourSheetOptions& opts);

  // Sample FieldStore |channel| onto |grid| at |time_sec|, then rebuild.
  bool rebuild_from_store(const FieldStore& store, FieldChannel channel,
                          const FieldGrid& grid, double time_sec,
                          const float* dem_meters,
                          const ContourSheetOptions& opts);

  void clear();

  const ContourSheetMesh& surface() const { return surface_; }
  // Line-list: 6 floats per segment (x0,y0,z0,x1,y1,z1).
  const std::vector<float>& curve_xyz() const { return curve_xyz_; }
  const ContourColorScale& color_scale() const { return color_scale_; }

  float field_value_min() const { return field_value_min_; }
  float field_value_max() const { return field_value_max_; }

  bool has_surface() const { return !surface_.empty(); }
  bool has_curves() const { return !curve_xyz_.empty(); }
  bool has_color_scale() const { return !color_scale_.empty(); }
  bool empty() const {
    return !has_surface() && !has_curves() && !has_color_scale();
  }

 private:
  ContourSheetMesh surface_;
  std::vector<float> curve_xyz_;
  ContourColorScale color_scale_;
  float field_value_min_ = 0.f;
  float field_value_max_ = 1.f;
};

// Short HUD title for a FieldChannel (ASCII; used by ContourColorScale).
VISTA_EXPORT const char* contour_channel_title(FieldChannel channel);

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_ATMOSPHERE_CONTOUR_CONTOUR_SHEET_H_
