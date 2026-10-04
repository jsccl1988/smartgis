// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_DIB_H_
#define SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_DIB_H_

#include <algorithm>
#include <cstdint>

#include "scenic/detail/err.h"

namespace scenic {
namespace detail {

// One top-down 32bpp DIBSection (BGRA) used as a map compose buffer.
struct Rhi2dSurface {
  HBITMAP bitmap = nullptr;
  void* bits = nullptr;
  int width = 0;
  int height = 0;
  uint32_t stride_bytes = 0;

  // Content version; bump when pixels change.
  uint64_t generation = 0;

  // Dirty rect union since the last clear_dirty() (optional tracking).
  bool has_dirty = false;
  int dirty_x = 0;
  int dirty_y = 0;
  int dirty_w = 0;
  int dirty_h = 0;

  void bump_generation() { ++generation; }

  void clear_dirty() {
    has_dirty = false;
    dirty_x = 0;
    dirty_y = 0;
    dirty_w = 0;
    dirty_h = 0;
  }

  // Unions |x,y,w,h| into the tracked dirty rect.
  void mark_dirty(int x, int y, int w, int h) {
    if (w < 1 || h < 1) {
      return;
    }
    if (!has_dirty) {
      dirty_x = x;
      dirty_y = y;
      dirty_w = w;
      dirty_h = h;
      has_dirty = true;
      return;
    }
    const int x2 = (std::max)(dirty_x + dirty_w, x + w);
    const int y2 = (std::max)(dirty_y + dirty_h, y + h);
    dirty_x = (std::min)(dirty_x, x);
    dirty_y = (std::min)(dirty_y, y);
    dirty_w = x2 - dirty_x;
    dirty_h = y2 - dirty_y;
  }
};

// Blit / present mode for HWND and buf→buf paths.
enum class Rhi2dBlitMode {
  kOpaque,    // BitBlt / StretchBlt (no color key)
  kColorKey,  // TransparentBlt or soft color-key compose
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_DIB_H_
