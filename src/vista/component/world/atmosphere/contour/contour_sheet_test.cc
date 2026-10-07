// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/atmosphere/contour/contour_sheet.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "vista/component/world/atmosphere/contour/contour_color_scale.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using vista::atmosphere::ContourColorScale;
  using vista::atmosphere::ContourScaleLayout;
  using vista::atmosphere::ContourScaleSide;
  using vista::atmosphere::ContourSheet;
  using vista::atmosphere::ContourSheetOptions;
  using vista::atmosphere::build_contour_color_scale;
  using vista::atmosphere::contour_scale_to_ndc;

  // Screen-ortho jet scale: ramp + NDC bar + title/tick text anchors.
  {
    ContourScaleLayout layout;
    layout.side = ContourScaleSide::kRight;
    layout.tick_count = 5;
    layout.ramp_pixels = 64;
    ContourColorScale scale;
    expect(build_contour_color_scale(10.f, 50.f, "Wave Hs", layout, &scale),
           "build color scale");
    expect(scale.ok && scale.ramp_h == 64 &&
               scale.ramp_rgba.size() == 64u * 8u * 4u,
           "ramp size");
    expect(scale.ticks.size() == 5u, "tick count");
    expect(scale.ortho_indices.size() == 6u, "bar tris");
    expect(scale.ortho_xyz.size() == 12u && scale.ortho_uv.size() == 8u,
           "bar verts");
    // Title + 5 ticks.
    expect(scale.label_text.size() == 6u && scale.label_xy.size() == 12u,
           "labels");
    expect(scale.label_text[0] == "Wave Hs", "title text");
    expect(std::fabs(scale.ticks.front().value - 10.f) < 1e-3f, "low tick");
    expect(std::fabs(scale.ticks.back().value - 50.f) < 1e-3f, "high tick");
    // Right-side bar: NDC x should be positive (right half).
    expect(scale.ortho_xyz[0] > 0.f && scale.ortho_xyz[3] > 0.f,
           "right side ndc");
    expect(std::fabs(contour_scale_to_ndc(0.5f)) < 1e-5f, "ndc mid");
  }

  // ContourSheet rebuild emits surface, curves, and side color scale.
  {
    constexpr int kC = 12;
    constexpr int kR = 10;
    std::vector<float> field(static_cast<size_t>(kC * kR), 0.f);
    for (int y = 0; y < kR; ++y) {
      for (int x = 0; x < kC; ++x) {
        field[static_cast<size_t>(y * kC + x)] =
            5.f + 20.f * static_cast<float>(x) / static_cast<float>(kC - 1);
      }
    }
    ContourSheetOptions opts;
    opts.curves = true;
    opts.surface = true;
    opts.color_scale = true;
    opts.dem_offset_m = 200.f;
    opts.value_to_meters = 100.f;
    opts.scale_title = "Hs";
    opts.scale_layout.side = ContourScaleSide::kRight;
    opts.scale_layout.tick_count = 4;

    ContourSheet sheet;
    expect(sheet.rebuild(field.data(), nullptr, kC, kR, 120.0, 30.0, 122.0,
                         32.0, opts),
           "rebuild sheet");
    expect(sheet.has_surface(), "has surface");
    expect(sheet.has_curves(), "has curves");
    expect(sheet.has_color_scale(), "has color scale");
    expect(sheet.color_scale().label_text[0] == "Hs", "sheet title");
    expect(sheet.field_value_min() < sheet.field_value_max(), "field range");
    // Curve segments are true 3D (6 floats each).
    expect((sheet.curve_xyz().size() % 6u) == 0u && !sheet.curve_xyz().empty(),
           "curve segments");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d contour_sheet_test fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "contour_sheet_test OK\n");
  return 0;
}
