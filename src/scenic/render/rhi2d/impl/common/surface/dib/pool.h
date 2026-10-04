// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_POOL_H_
#define SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_POOL_H_

#include <mutex>
#include <vector>

#include "scenic/render/rhi2d/impl/common/surface/dib/dib.h"

namespace scenic {
namespace detail {

// Reuses DIBSections by exact (width, height).
class Rhi2dSurfacePool {
 public:
  Rhi2dSurfacePool() = default;
  ~Rhi2dSurfacePool();

  Rhi2dSurfacePool(const Rhi2dSurfacePool&) = delete;
  Rhi2dSurfacePool& operator=(const Rhi2dSurfacePool&) = delete;

  // Creates or reuses a surface. On failure returns an empty Rhi2dSurface.
  Rhi2dSurface acquire(HWND hwnd, int width, int height);

  // Returns a surface to the pool (keeps bitmap alive). Null bitmap is a no-op.
  void release(Rhi2dSurface surface);

  void clear();

 private:
  struct Entry {
    Rhi2dSurface surface;
    bool free = true;
  };

  std::mutex lock_;
  std::vector<Entry> entries_;
};

// Process-wide pool for owned Rhi2dOwnedSurface instances.
Rhi2dSurfacePool& rhi2d_surface_pool();

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_GDI_SURFACE_DIB_POOL_H_
