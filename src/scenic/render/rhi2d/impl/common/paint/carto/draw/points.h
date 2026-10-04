// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_POINTS_H_
#define SCENIC_GDI_POINTS_H_

#include <cstddef>

#include "base/memory/arena.h"
#include "scenic/detail/err.h"

namespace scenic {
namespace detail {

// Scratch POINT[] for GDI draw paths. Use process heap via base::allocate
// (not tls_allocate): HybridOptimized TLS pools have crashed on the
// NThreadPool worker thread (corrupt PMR iterators).
inline POINT* alloc_gdi_points(int count) {
  if (count <= 0) {
    return nullptr;
  }
  const size_t bytes = sizeof(POINT) * static_cast<size_t>(count);
  return static_cast<POINT*>(base::allocate(bytes));
}

inline void free_gdi_points(POINT* points, int count) {
  if (!points || count <= 0) {
    return;
  }
  const size_t bytes = sizeof(POINT) * static_cast<size_t>(count);
  base::deallocate(points, bytes);
}

// RAII wrapper for alloc_gdi_points / free_gdi_points.
struct ScopedGdiPoints {
  POINT* data = nullptr;
  int count = 0;

  explicit ScopedGdiPoints(int n) : data(alloc_gdi_points(n)), count(n) {}
  ~ScopedGdiPoints() { free_gdi_points(data, count); }

  ScopedGdiPoints(const ScopedGdiPoints&) = delete;
  ScopedGdiPoints& operator=(const ScopedGdiPoints&) = delete;

  [[nodiscard]] bool ok() const { return data != nullptr || count <= 0; }
  POINT* get() const { return data; }
  explicit operator bool() const { return data != nullptr; }
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_POINTS_H_
