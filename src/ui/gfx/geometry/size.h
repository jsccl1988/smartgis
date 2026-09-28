// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_GEOMETRY_SIZE_H_
#define UI_GFX_GEOMETRY_SIZE_H_

namespace ui {
namespace gfx {

// Integer width/height for shell layout and text measurement.
struct Size {
  int width = 0;
  int height = 0;

  bool is_empty() const { return width <= 0 || height <= 0; }
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_GEOMETRY_SIZE_H_
