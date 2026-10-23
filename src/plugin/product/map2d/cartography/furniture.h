// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Neatline and scale bar for the current geographic view. Layout is pure;
// paint goes through ui::gfx::Canvas (Views + Skia/GDI backend).

#ifndef PLUGIN_MAP2D_CARTOGRAPHY_FURNITURE_H_
#define PLUGIN_MAP2D_CARTOGRAPHY_FURNITURE_H_

#include <string>

namespace ui {
namespace gfx {
class Canvas;
}
}  // namespace ui

namespace plugin {

struct MapViewBox {
  double west = -180;
  double south = -85;
  double east = 180;
  double north = 85;
  int width_px = 0;
  int height_px = 0;
};

// Which chrome pieces paint_furniture draws. Defaults match the historical
// always-on neatline, scale bar, and north arrow. Eagle eye is not a field.
struct FurnitureOptions {
  bool scale_bar = true;
  bool north_arrow = true;
  bool border = true;
};

// Inner/outer neatline plus one scale bar driven by the view width.
struct FurnitureLayout {
  int outer_x = 0;
  int outer_y = 0;
  int outer_w = 0;
  int outer_h = 0;
  int inner_x = 0;
  int inner_y = 0;
  int inner_w = 0;
  int inner_h = 0;
  int bar_x = 0;
  int bar_y = 0;
  int bar_len_px = 0;
  double bar_meters = 0;
  double scale_denominator = 0;
  bool valid = false;
};

FurnitureLayout layout_furniture(const MapViewBox& view, double dpi = 96.0);

std::string scale_ratio_label(double denominator);
std::string scale_bar_label(double meters);

void paint_furniture(ui::gfx::Canvas* canvas, const FurnitureLayout& layout,
                     const FurnitureOptions& options = {});

}  // namespace plugin

#endif  // PLUGIN_MAP2D_CARTOGRAPHY_FURNITURE_H_
