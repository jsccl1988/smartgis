// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/cartography/furniture.h"

#include "content/browser/glang.runtime/host/tile/web_mercator.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace plugin {
namespace {

constexpr double k_pi = 3.14159265358979323846;

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

double ground_meters(const MapViewBox& view) {
  const double span = std::abs(view.east - view.west);
  if (!(span > 0.0)) {
    return 0;
  }
  const double lat = std::clamp((view.south + view.north) * 0.5,
                                -content::tile::k_max_mercator_lat,
                                content::tile::k_max_mercator_lat);
  const double lat_rad = lat * k_pi / 180.0;
  return std::cos(lat_rad) * 2.0 * k_pi * content::tile::k_earth_radius_m *
         (span / 360.0);
}

std::wstring widen(const std::string& text) {
  std::wstring out;
  out.reserve(text.size());
  for (unsigned char ch : text) {
    out.push_back(static_cast<wchar_t>(ch));
  }
  return out;
}

}  // namespace

std::string scale_ratio_label(double denominator) {
  if (!(denominator > 1.0)) {
    return "1:1";
  }
  const double nice = nice_step(denominator);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "1:%.0f", nice);
  return buf;
}

std::string scale_bar_label(double meters) {
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

FurnitureLayout layout_furniture(const MapViewBox& view, double dpi) {
  FurnitureLayout layout;
  if (view.width_px < 80 || view.height_px < 80) {
    return layout;
  }
  constexpr int k_outer = 8;
  constexpr int k_gap = 4;
  layout.outer_x = k_outer;
  layout.outer_y = k_outer;
  layout.outer_w = view.width_px - k_outer * 2;
  layout.outer_h = view.height_px - k_outer * 2;
  layout.inner_x = k_outer + k_gap;
  layout.inner_y = k_outer + k_gap;
  layout.inner_w = layout.outer_w - k_gap * 2;
  layout.inner_h = layout.outer_h - k_gap * 2;
  if (layout.inner_w < 32 || layout.inner_h < 32) {
    return layout;
  }
  const double meters = ground_meters(view);
  const double m_per_px = meters / static_cast<double>(view.width_px);
  if (!(m_per_px > 0.0)) {
    return layout;
  }
  if (!(dpi > 1.0)) {
    dpi = 96.0;
  }
  const double paper_m_per_px = 0.0254 / dpi;
  layout.scale_denominator = m_per_px / paper_m_per_px;
  constexpr int k_target_px = 96;
  layout.bar_meters = nice_step(m_per_px * static_cast<double>(k_target_px));
  layout.bar_len_px =
      std::max(24, static_cast<int>(std::lround(layout.bar_meters / m_per_px)));
  if (layout.bar_len_px > layout.inner_w / 2) {
    layout.bar_len_px = std::max(24, layout.inner_w / 3);
    layout.bar_meters = m_per_px * static_cast<double>(layout.bar_len_px);
  }
  layout.bar_x = layout.inner_x + 12;
  layout.bar_y = layout.inner_y + layout.inner_h - 28;
  layout.valid = true;
  return layout;
}

void paint_furniture(ui::gfx::Canvas* canvas, const FurnitureLayout& layout,
                     const FurnitureOptions& options) {
  if (!canvas || !layout.valid) {
    return;
  }
  const ui::gfx::Color ink = ui::gfx::color_rgb(20, 20, 20);
  const ui::gfx::Color hair = ui::gfx::color_rgb(60, 60, 60);
  if (options.border) {
    canvas->stroke_rect(layout.outer_x, layout.outer_y, layout.outer_w,
                        layout.outer_h, ink, 2);
    canvas->stroke_rect(layout.inner_x, layout.inner_y, layout.inner_w,
                        layout.inner_h, hair, 1);
  }
  if (options.scale_bar) {
    const int x0 = layout.bar_x;
    const int y0 = layout.bar_y;
    const int x1 = x0 + layout.bar_len_px;
    canvas->draw_line(x0, y0, x1, y0, ink, 3);
    canvas->draw_line(x0, y0 - 5, x0, y0 + 5, ink, 2);
    canvas->draw_line(x1, y0 - 5, x1, y0 + 5, ink, 2);
    const std::wstring bar = widen(scale_bar_label(layout.bar_meters));
    const std::wstring ratio = widen(scale_ratio_label(layout.scale_denominator));
    canvas->draw_text(x1 + 8, y0 - 8, bar.c_str(), ink);
    canvas->draw_text(x0, y0 + 8, ratio.c_str(), ink);
  }
  if (options.north_arrow) {
    const int nx = layout.inner_x + layout.inner_w / 2;
    const int ny = layout.inner_y + 16;
    canvas->draw_line(nx, ny + 14, nx, ny, ink, 2);
    canvas->draw_line(nx - 6, ny + 6, nx, ny, ink, 2);
    canvas->draw_line(nx + 6, ny + 6, nx, ny, ink, 2);
    canvas->draw_text(nx + 8, ny, L"N", ink);
  }
}

}  // namespace plugin
