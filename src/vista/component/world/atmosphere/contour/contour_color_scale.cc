// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/atmosphere/contour/contour_color_scale.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "vista/terrain/dem/dem_contour.h"

namespace vista {
namespace atmosphere {
namespace {

void format_tick_label(float value, char* buf, std::size_t cap) {
  if (!buf || cap < 2) {
    return;
  }
  const float av = std::fabs(value);
  if (av >= 1000.f || (av > 0.f && av < 0.01f)) {
    std::snprintf(buf, cap, "%.2e", static_cast<double>(value));
  } else if (av >= 100.f) {
    std::snprintf(buf, cap, "%.0f", static_cast<double>(value));
  } else if (av >= 10.f) {
    std::snprintf(buf, cap, "%.1f", static_cast<double>(value));
  } else {
    std::snprintf(buf, cap, "%.2f", static_cast<double>(value));
  }
}

void push_xy(std::vector<float>* xy, float x, float y) {
  xy->push_back(x);
  xy->push_back(y);
}

void push_xyz(std::vector<float>* xyz, float x, float y) {
  xyz->push_back(x);
  xyz->push_back(y);
  xyz->push_back(0.f);
}

}  // namespace

bool build_contour_color_scale(float value_min, float value_max,
                               const char* title,
                               const ContourScaleLayout& layout,
                               ContourColorScale* out) {
  if (!out) {
    return false;
  }
  *out = ContourColorScale{};
  if (!(value_max > value_min) || !std::isfinite(value_min) ||
      !std::isfinite(value_max)) {
    return false;
  }
  ContourScaleLayout lay = layout;
  lay.tick_count = (std::max)(2, (std::min)(16, lay.tick_count));
  lay.ramp_pixels = (std::max)(8, (std::min)(1024, lay.ramp_pixels));
  lay.margin = (std::max)(0.f, (std::min)(0.4f, lay.margin));
  lay.bar_width = (std::max)(0.008f, (std::min)(0.12f, lay.bar_width));
  lay.bar_height = (std::max)(0.1f, (std::min)(0.9f, lay.bar_height));
  lay.bar_center_y = (std::max)(0.1f, (std::min)(0.9f, lay.bar_center_y));
  lay.label_gap = (std::max)(0.f, (std::min)(0.1f, lay.label_gap));

  out->value_min = value_min;
  out->value_max = value_max;
  out->layout = lay;
  if (title && title[0]) {
    out->title = title;
  }

  const float span = value_max - value_min;
  out->ticks.resize(static_cast<std::size_t>(lay.tick_count));
  for (int i = 0; i < lay.tick_count; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(lay.tick_count - 1);
    ContourScaleTick& tick = out->ticks[static_cast<std::size_t>(i)];
    tick.t01 = t;
    tick.value = value_min + t * span;
    format_tick_label(tick.value, tick.label, sizeof(tick.label));
  }

  // Width ≥ 8 so Scene3dOverlays::tin_has_drape accepts the strip.
  out->ramp_w = 8;
  out->ramp_h = lay.ramp_pixels;
  out->ramp_rgba.assign(
      static_cast<std::size_t>(out->ramp_w) *
          static_cast<std::size_t>(lay.ramp_pixels) * 4u,
      0);
  for (int y = 0; y < lay.ramp_pixels; ++y) {
    const float t01 =
        static_cast<float>(y) / static_cast<float>(lay.ramp_pixels - 1);
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    jet_elevation_rgb(t01, &r, &g, &b);
    const uint8_t ru = static_cast<uint8_t>(
        (std::max)(0, (std::min)(255, static_cast<int>(r * 255.f + 0.5f))));
    const uint8_t gu = static_cast<uint8_t>(
        (std::max)(0, (std::min)(255, static_cast<int>(g * 255.f + 0.5f))));
    const uint8_t bu = static_cast<uint8_t>(
        (std::max)(0, (std::min)(255, static_cast<int>(b * 255.f + 0.5f))));
    for (int x = 0; x < out->ramp_w; ++x) {
      const std::size_t o =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(out->ramp_w) +
           static_cast<std::size_t>(x)) *
          4u;
      out->ramp_rgba[o + 0] = ru;
      out->ramp_rgba[o + 1] = gu;
      out->ramp_rgba[o + 2] = bu;
      out->ramp_rgba[o + 3] = 255;
    }
  }

  const float bar_bottom = lay.bar_center_y - 0.5f * lay.bar_height;
  const float bar_top = lay.bar_center_y + 0.5f * lay.bar_height;
  float bar_left = 0.f;
  float bar_right = 0.f;
  float label_x = 0.f;
  if (lay.side == ContourScaleSide::kRight) {
    bar_right = 1.f - lay.margin;
    bar_left = bar_right - lay.bar_width;
    label_x = bar_left - lay.label_gap;
  } else {
    bar_left = lay.margin;
    bar_right = bar_left + lay.bar_width;
    label_x = bar_right + lay.label_gap;
  }

  const float x0 = contour_scale_to_ndc(bar_left);
  const float x1 = contour_scale_to_ndc(bar_right);
  const float y0 = contour_scale_to_ndc(bar_bottom);
  const float y1 = contour_scale_to_ndc(bar_top);

  out->ortho_xyz.clear();
  out->ortho_uv.clear();
  out->ortho_indices.clear();
  out->ortho_xyz.reserve(12u);
  out->ortho_uv.reserve(8u);
  // CCW: BL, BR, TR, TL — v=0 at low / bottom.
  push_xyz(&out->ortho_xyz, x0, y0);
  out->ortho_uv.push_back(0.f);
  out->ortho_uv.push_back(0.f);
  push_xyz(&out->ortho_xyz, x1, y0);
  out->ortho_uv.push_back(1.f);
  out->ortho_uv.push_back(0.f);
  push_xyz(&out->ortho_xyz, x1, y1);
  out->ortho_uv.push_back(1.f);
  out->ortho_uv.push_back(1.f);
  push_xyz(&out->ortho_xyz, x0, y1);
  out->ortho_uv.push_back(0.f);
  out->ortho_uv.push_back(1.f);
  out->ortho_indices = {0, 1, 2, 0, 2, 3};

  out->label_xy.clear();
  out->label_text.clear();
  const float label_ndc_x = contour_scale_to_ndc(label_x);
  if (!out->title.empty()) {
    const float title_y = (std::min)(0.98f, bar_top + 0.04f);
    push_xy(&out->label_xy, label_ndc_x, contour_scale_to_ndc(title_y));
    out->label_text.push_back(out->title);
  }
  for (const ContourScaleTick& tick : out->ticks) {
    const float vy = bar_bottom + tick.t01 * (bar_top - bar_bottom);
    push_xy(&out->label_xy, label_ndc_x, contour_scale_to_ndc(vy));
    out->label_text.emplace_back(tick.label);
  }

  out->ok = !out->ramp_rgba.empty() && out->ortho_indices.size() == 6u;
  return out->ok;
}

}  // namespace atmosphere
}  // namespace vista
