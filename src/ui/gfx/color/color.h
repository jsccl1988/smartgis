// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_COLOR_COLOR_H_
#define UI_GFX_COLOR_COLOR_H_

#include <cstdint>

namespace ui {
namespace gfx {

// Packed ARGB (not a Skia Color4f). Software shell paint only.
using Color = std::uint32_t;

inline Color color_argb(std::uint8_t a,
                        std::uint8_t r,
                        std::uint8_t g,
                        std::uint8_t b) {
  return (static_cast<Color>(a) << 24) | (static_cast<Color>(r) << 16) |
         (static_cast<Color>(g) << 8) | static_cast<Color>(b);
}

inline Color color_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return color_argb(255, r, g, b);
}

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_COLOR_COLOR_H_
