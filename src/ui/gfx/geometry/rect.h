// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_GEOMETRY_RECT_H_
#define UI_GFX_GEOMETRY_RECT_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {

// Axis-aligned integer rectangle (x, y, width, height). Shell layout only.
struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  int right() const { return x + width; }
  int bottom() const { return y + height; }

  Point origin() const { return Point{x, y}; }
  Size size() const { return Size{width, height}; }

  bool is_empty() const { return width <= 0 || height <= 0; }

  bool contains(int px, int py) const {
    return px >= x && py >= y && px < right() && py < bottom();
  }

  bool contains(const Point& p) const { return contains(p.x, p.y); }

  bool intersects(const Rect& other) const {
    return width > 0 && height > 0 && other.width > 0 && other.height > 0 &&
           x < other.right() && other.x < right() && y < other.bottom() &&
           other.y < bottom();
  }
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_GEOMETRY_RECT_H_
