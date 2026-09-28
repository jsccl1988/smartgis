// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/image/image.h"

namespace ui {
namespace gfx {

Image::Image(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  width_ = width;
  height_ = height;
  pixels_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u,
                 0);
}

const std::uint8_t* Image::pixels() const {
  return empty() ? nullptr : pixels_.data();
}

std::uint8_t* Image::pixels() {
  return empty() ? nullptr : pixels_.data();
}

void Image::clear(std::uint32_t argb) {
  if (empty()) {
    return;
  }
  const std::uint8_t a = static_cast<std::uint8_t>((argb >> 24) & 0xFF);
  const std::uint8_t r = static_cast<std::uint8_t>((argb >> 16) & 0xFF);
  const std::uint8_t g = static_cast<std::uint8_t>((argb >> 8) & 0xFF);
  const std::uint8_t b = static_cast<std::uint8_t>(argb & 0xFF);
  for (size_t i = 0; i + 3 < pixels_.size(); i += 4) {
    pixels_[i] = b;
    pixels_[i + 1] = g;
    pixels_[i + 2] = r;
    pixels_[i + 3] = a;
  }
}

}  // namespace gfx
}  // namespace ui
