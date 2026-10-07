// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_ATMOSPHERE_CONTOUR_CONTOUR_COLOR_SCALE_H_
#define VISTA_COMPONENT_WORLD_ATMOSPHERE_CONTOUR_CONTOUR_COLOR_SCALE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"

namespace vista {
namespace atmosphere {

// Which viewport edge hosts the orthographic color scale.
enum class ContourScaleSide {
  kRight = 0,
  kLeft = 1,
};

// Screen-normalized (0..1, origin bottom-left) layout for a side color bar.
// Hosts map this into NDC / pixel quads for an orthographic HUD pass.
struct ContourScaleLayout {
  ContourScaleSide side = ContourScaleSide::kRight;
  float margin = 0.03f;       // gap from the chosen side edge
  float bar_width = 0.028f;   // fraction of viewport width
  float bar_height = 0.48f;   // fraction of viewport height
  float bar_center_y = 0.52f; // vertical center (0..1)
  float label_gap = 0.012f;   // gap between bar and tick text
  int tick_count = 6;         // including low and high ends (2..16)
  int ramp_pixels = 256;      // vertical RGBA strip resolution
};

// One labeled stop along the jet ramp (t01=0 low / bottom, 1 high / top).
struct ContourScaleTick {
  float value = 0.f;
  float t01 = 0.f;
  char label[32] = {};
};

// Orthographic color-scale IR: jet ramp texture + NDC bar mesh + text anchors.
// Drawn in screen space (not perspective); side placement matches Origin HUD.
struct ContourColorScale {
  bool ok = false;
  float value_min = 0.f;
  float value_max = 1.f;
  std::string title;
  ContourScaleLayout layout;

  std::vector<ContourScaleTick> ticks;

  // Vertical jet strip, row 0 = low (bottom), last row = high (top). RGBA8.
  std::vector<uint8_t> ramp_rgba;
  int ramp_w = 1;
  int ramp_h = 0;

  // Screen-ortho bar: leftover-style XYZ with Z=0, XY in NDC (-1..1).
  // UVs sample the ramp (v=0 bottom / low). Two triangles.
  std::vector<float> ortho_xyz;
  std::vector<float> ortho_uv;
  std::vector<uint32_t> ortho_indices;

  // Text anchors in NDC (x,y per entry). |label_text| parallels |label_xy|/2.
  // Index 0 is the title (above the bar); following entries are ticks.
  std::vector<float> label_xy;
  std::vector<std::string> label_text;

  bool empty() const { return !ok || ramp_rgba.empty(); }
};

// Build jet color scale for |value_min|…|value_max|. |title| may be null/empty.
VISTA_EXPORT bool build_contour_color_scale(float value_min, float value_max,
                                            const char* title,
                                            const ContourScaleLayout& layout,
                                            ContourColorScale* out);

// Map viewport fraction (0..1 bottom-left) to NDC (-1..1).
inline float contour_scale_to_ndc(float v01) {
  return v01 * 2.f - 1.f;
}

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_ATMOSPHERE_CONTOUR_CONTOUR_COLOR_SCALE_H_
