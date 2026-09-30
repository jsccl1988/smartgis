// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_SURFACE_POOL_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_SURFACE_POOL_H_

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

#include "legacy/core/macros/macros.h"

namespace render {

enum eSwapType {
  BLT_STRETCH,
  BLT_TRANSPARENT,
};

// One pooled top-down 32bpp DIBSection (BGRA) for map compose buffers.
struct GdiSurface {
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

// Reuses DIBSections by exact (width, height).
class GdiSurfacePool {
 public:
  GdiSurfacePool() = default;
  ~GdiSurfacePool();

  GdiSurfacePool(const GdiSurfacePool&) = delete;
  GdiSurfacePool& operator=(const GdiSurfacePool&) = delete;

  // Creates or reuses a surface. On failure returns an empty GdiSurface.
  GdiSurface acquire(HWND hwnd, int width, int height);

  // Returns a surface to the pool (keeps bitmap alive). Null bitmap is a no-op.
  void release(GdiSurface surface);

  void clear();

 private:
  struct Entry {
    GdiSurface surface;
    bool free = true;
  };

  std::mutex lock_;
  std::vector<Entry> entries_;
};

// Process-wide pool for owned GdiOwnedSurface instances.
GdiSurfacePool& gdi_surface_pool();

// RAII holder for a pooled (or shared) map surface + optional paint DC.
// Replaces the former SmtRenderBuf type.
class GdiOwnedSurface {
 public:
  GdiOwnedSurface() = default;
  explicit GdiOwnedSurface(HWND hwnd);
  ~GdiOwnedSurface();

  GdiOwnedSurface(const GdiOwnedSurface&) = delete;
  GdiOwnedSurface& operator=(const GdiOwnedSurface&) = delete;

  // Releases current, then set_wnd + set_size from |other| (owned copy).
  GdiOwnedSurface& assign_size_from(const GdiOwnedSurface& other);

  long set_wnd(HWND hwnd);
  HWND wnd() const { return hwnd_; }

  long set_size(int cx, int cy);
  int width() const { return surface_.width; }
  int height() const { return surface_.height; }

  HBITMAP bitmap() const { return surface_.bitmap; }
  void* bits() const { return surface_.bits; }
  uint32_t stride_bytes() const { return surface_.stride_bytes; }
  GdiSurface& surface() { return surface_; }
  const GdiSurface& surface() const { return surface_; }

  // Non-owning alias of |src| (destructor will not release).
  long share_from(const GdiOwnedSurface& src);

  long clear(int x, int y, int w, int h, COLORREF clr = RGB(170, 211, 223));

  // Present this surface to its HWND.
  long present(int dest_org_x, int dest_org_y, int dest_w, int dest_h,
               int src_org_x, int src_org_y, int op = SRCCOPY);
  long present(int dest_org_x, int dest_org_y, int dest_w, int dest_h,
               int src_org_x, int src_org_y, int src_w, int src_h,
               eSwapType type = BLT_TRANSPARENT, int op = SRCCOPY,
               // Key must NOT match clear ocean (170,211,223) or the HWND
               // stays white when the whole buffer matches the key.
               COLORREF clr = RGB(255, 255, 255));

  // Buf→buf compose (blend_render_pass) with GDI BitBlt/TransparentBlt
  // fallback.
  long blit_to(GdiOwnedSurface& target, int dest_org_x, int dest_org_y,
               int dest_w, int dest_h, int src_org_x, int src_org_y,
               int op = SRCCOPY);
  long blit_to(GdiOwnedSurface& target, int dest_org_x, int dest_org_y,
               int dest_w, int dest_h, int src_org_x, int src_org_y, int src_w,
               int src_h, eSwapType type = BLT_TRANSPARENT, int op = SRCCOPY,
               COLORREF clr = RGB(255, 255, 255));

  HDC prepare_dc(bool clip = true);
  long end_dc();

 private:
  void release_owned();

  HWND hwnd_ = nullptr;
  GdiSurface surface_{};
  bool owned_ = true;
  HDC paint_dc_ = nullptr;
  HBITMAP old_bitmap_ = nullptr;

#ifdef SMT_THREAD_SAFE
  std::mutex lock_;
#endif
};

}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_SURFACE_POOL_H_
