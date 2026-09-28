// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_RASTER_SHELL_RASTER_H_
#define UI_GFX_RASTER_SHELL_RASTER_H_

#include <cstdint>

namespace ui {
namespace gfx {

// Rasterized application shell (Views menus and panels). BGRA8, top-down,
// tightly packed or padded by stride_bytes. gpu::draw_and_swap consumes this
// as one quad over the map; this type does not present or blend.
struct ShellRaster {
  const std::uint8_t* bgra = nullptr;
  std::uint32_t width_px = 0;
  std::uint32_t height_px = 0;
  std::uint32_t stride_bytes = 0;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_RASTER_SHELL_RASTER_H_
