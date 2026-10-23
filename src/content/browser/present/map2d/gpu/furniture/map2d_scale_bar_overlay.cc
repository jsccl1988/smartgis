// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/gpu/furniture/map2d_scale_bar_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <string>

namespace content {
namespace detail {
namespace {

constexpr uint32_t kInkRgba = 0xFF141414u;
constexpr uint32_t kHaloRgba = 0xFFFFFFFFu;
constexpr float kBarHalfPx = 1.5f;
constexpr float kTextSizePx = 12.f;

double nice_step(double value) {
  if (!(value > 0.0)) {
    return 1.0;
  }
  const double exp = std::floor(std::log10(value));
  const double base = std::pow(10.0, exp);
  const double n = value / base;
  const double nice = (n < 1.5) ? 1.0 : (n < 3.5) ? 2.0 : (n < 7.5) ? 5.0 : 10.0;
  return nice * base;
}

bool extent_is_lonlat(double xmin, double ymin, double xmax, double ymax) {
  return xmin >= -180.5 && xmax <= 180.5 && ymin >= -90.5 && ymax <= 90.5;
}

double ground_width_m(double min_x, double min_y, double max_x, double max_y) {
  const double width = max_x - min_x;
  if (!(width > 0.0)) {
    return 0.0;
  }
  if (!extent_is_lonlat(min_x, min_y, max_x, max_y)) {
    return width;
  }
  const double center_lat = (min_y + max_y) * 0.5;
  const double cos_lat =
      std::cos(center_lat * std::numbers::pi / 180.0);
  const double meters_per_degree = 111320.0 * std::max(cos_lat, 0.01);
  return width * meters_per_degree;
}

std::string bar_label(double meters) {
  char buf[64];
  if (meters >= 1000.0) {
    const double km = meters / 1000.0;
    if (km >= 10.0) {
      std::snprintf(buf, sizeof(buf), "%.0f km", km);
    } else {
      std::snprintf(buf, sizeof(buf), "%.1f km", km);
    }
  } else {
    std::snprintf(buf, sizeof(buf), "%.0f m", meters);
  }
  return buf;
}

std::string ratio_label(double denominator) {
  if (!(denominator > 1.0)) {
    return "1:1";
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "1:%.0f", nice_step(denominator));
  return buf;
}

void append_pixel_quad(vista::MapIR* ir, float x0, float y0, float x1, float y1,
                       uint32_t rgba) {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kLine;
  item.pixel_space = true;
  item.rgba = rgba;
  item.opacity = 1.f;
  item.vertices = {
      {x0, y0, 0, 0, 0},
      {x1, y0, 0, 0, 0},
      {x1, y1, 0, 0, 0},
      {x0, y1, 0, 0, 0},
  };
  item.indices = {0, 1, 2, 0, 2, 3};
  ir->items.push_back(std::move(item));
}

void append_ascii_run(vista::MapIR* ir, float x, float y, const std::string& text,
                      float size_px, uint32_t rgba) {
  float pen = x;
  for (unsigned char ch : text) {
    if (ch < 0x20 || ch > 0x7e) {
      continue;
    }
    const float adv = size_px * 0.55f;
    vista::DrawItem glyph;
    glyph.kind = vista::DrawKind::kText;
    glyph.pixel_space = true;
    glyph.codepoint = ch;
    glyph.text_size_px = size_px;
    glyph.font = vista::MapFont::kGothic;
    glyph.halo_width_px = 1.5f;
    glyph.halo_rgba = kHaloRgba;
    glyph.rgba = rgba;
    glyph.opacity = 1.f;
    glyph.anchor_x = pen + adv * 0.5f;
    glyph.anchor_y = y + size_px * 0.5f;
    glyph.vertices = {
        {pen, y, 0, 0, 0},
        {pen + adv, y, 0, 1, 0},
        {pen + adv, y + size_px, 0, 1, 1},
        {pen, y + size_px, 0, 0, 1},
    };
    glyph.indices = {0, 1, 2, 0, 2, 3};
    ir->items.push_back(std::move(glyph));
    pen += adv;
  }
}

}  // namespace

void append_map2d_scale_bar(uint32_t width_px, uint32_t height_px, double min_x,
                            double min_y, double max_x, double max_y,
                            vista::MapIR* ir) {
  if (!ir || width_px < 80 || height_px < 80) {
    return;
  }
  const double ground_m = ground_width_m(min_x, min_y, max_x, max_y);
  if (!(ground_m > 0.0)) {
    return;
  }
  const double m_per_px = ground_m / static_cast<double>(width_px);
  constexpr double kPaperMPerPx = 0.0254 / 96.0;
  const double denominator = m_per_px / kPaperMPerPx;
  constexpr int kTargetPx = 96;
  double bar_m = nice_step(m_per_px * static_cast<double>(kTargetPx));
  int bar_len =
      std::max(24, static_cast<int>(std::lround(bar_m / m_per_px)));
  const int max_len = static_cast<int>(width_px) / 3;
  if (bar_len > max_len) {
    bar_len = std::max(24, max_len);
    bar_m = m_per_px * static_cast<double>(bar_len);
  }

  const float x0 = 16.f;
  const float y0 = static_cast<float>(height_px) - 28.f;
  const float x1 = x0 + static_cast<float>(bar_len);
  // Horizontal bar + end ticks (pixel-space line quads).
  append_pixel_quad(ir, x0, y0 - kBarHalfPx, x1, y0 + kBarHalfPx, kInkRgba);
  append_pixel_quad(ir, x0 - 1.f, y0 - 6.f, x0 + 1.f, y0 + 6.f, kInkRgba);
  append_pixel_quad(ir, x1 - 1.f, y0 - 6.f, x1 + 1.f, y0 + 6.f, kInkRgba);
  append_ascii_run(ir, x1 + 8.f, y0 - 10.f, bar_label(bar_m), kTextSizePx,
                   kInkRgba);
  append_ascii_run(ir, x0, y0 + 8.f, ratio_label(denominator), kTextSizePx,
                   kInkRgba);
}

}  // namespace detail
}  // namespace content
