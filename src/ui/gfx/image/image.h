// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_IMAGE_IMAGE_H_
#define UI_GFX_IMAGE_IMAGE_H_

#include "ui/ui_views_export.h"
#include <cstdint>
#include <vector>

#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {

// Thin BGRA8 bitmap for shell chrome (icons, badges). Not a GIS texture,
// not ImageSkia, and not a decode pipeline.
class UI_VIEWS_EXPORT Image {
 public:
  Image() = default;
  Image(int width, int height);

  bool empty() const { return width_ <= 0 || height_ <= 0 || pixels_.empty(); }
  int width() const { return width_; }
  int height() const { return height_; }
  Size size() const { return Size{width_, height_}; }

  // Tightly packed BGRA8, top-down. Null when empty.
  const std::uint8_t* pixels() const;
  std::uint8_t* pixels();
  int stride_bytes() const { return width_ * 4; }

  // Fill every pixel with packed ARGB (same packing as ui::gfx::Color).
  void clear(std::uint32_t argb);

 private:
  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> pixels_;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_IMAGE_IMAGE_H_
