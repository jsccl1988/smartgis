// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_COMMON_SURFACE_DIB_OWNED_H_
#define SCENIC_RHI2D_IMPL_COMMON_SURFACE_DIB_OWNED_H_

#include <mutex>

#include "scenic/render/rhi2d/impl/common/surface/dib/pool.h"

namespace scenic {
namespace detail {

// RAII holder for a pooled (or shared) map surface + optional paint DC.
// Lifecycle only — HWND present / buf→buf compose live in surface/composer/.
class Rhi2dOwnedSurface {
 public:
  Rhi2dOwnedSurface() = default;
  explicit Rhi2dOwnedSurface(HWND hwnd);
  ~Rhi2dOwnedSurface();

  Rhi2dOwnedSurface(const Rhi2dOwnedSurface&) = delete;
  Rhi2dOwnedSurface& operator=(const Rhi2dOwnedSurface&) = delete;

  // Releases current, then set_wnd + set_size from |other| (owned copy).
  Rhi2dOwnedSurface& assign_size_from(const Rhi2dOwnedSurface& other);

  long set_wnd(HWND hwnd);
  HWND wnd() const { return hwnd_; }

  long set_size(int cx, int cy);
  int width() const { return surface_.width; }
  int height() const { return surface_.height; }

  HBITMAP bitmap() const { return surface_.bitmap; }
  void* bits() const { return surface_.bits; }
  uint32_t stride_bytes() const { return surface_.stride_bytes; }
  Rhi2dSurface& surface() { return surface_; }
  const Rhi2dSurface& surface() const { return surface_; }

  // Non-owning alias of |src| (destructor will not release).
  long share_from(const Rhi2dOwnedSurface& src);

  long clear(int x, int y, int w, int h, COLORREF clr = RGB(170, 211, 223));

  HDC prepare_dc(bool clip = true);
  long end_dc();

 private:
  void release_owned();

  HWND hwnd_ = nullptr;
  Rhi2dSurface surface_{};
  bool owned_ = true;
  HDC paint_dc_ = nullptr;
  HBITMAP old_bitmap_ = nullptr;

  std::mutex lock_;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_COMMON_SURFACE_DIB_OWNED_H_
