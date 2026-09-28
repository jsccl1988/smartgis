// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_GEOMETRY_POINT_H_
#define UI_GFX_GEOMETRY_POINT_H_

namespace ui {
namespace gfx {

// Integer pixel / DIP point for the Views shell. Not a GIS coordinate.
struct Point {
  int x = 0;
  int y = 0;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_GEOMETRY_POINT_H_
