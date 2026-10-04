// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/surface/dib/pool.h"

#include <cstring>

namespace scenic {
namespace detail {
namespace {

bool create_dib_section(HWND hwnd, int width, int height, Rhi2dSurface* out) {
  if (!out || width < 1 || height < 1) {
    return false;
  }
  BITMAPINFO bmi;
  std::memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  // Prefer a screen DC when available; nullptr is valid for BI_RGB DIBs and
  // avoids GetDC(HWND) when the pool is touched off the HWND thread.
  HDC hdc = hwnd ? ::GetDC(hwnd) : nullptr;
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (hdc) {
    ::ReleaseDC(hwnd, hdc);
  }
  if (!bmp || !bits) {
    if (bmp) {
      DeleteObject(bmp);
    }
    return false;
  }
  out->bitmap = bmp;
  out->bits = bits;
  out->width = width;
  out->height = height;
  out->stride_bytes = static_cast<uint32_t>(width) * 4u;
  return true;
}

}  // namespace

Rhi2dSurfacePool::~Rhi2dSurfacePool() { clear(); }

Rhi2dSurface Rhi2dSurfacePool::acquire(HWND hwnd, int width, int height) {
  Rhi2dSurface empty;
  if (width < 1 || height < 1) {
    return empty;
  }
  std::lock_guard<std::mutex> guard(lock_);
  for (Entry& e : entries_) {
    if (e.free && e.surface.width == width && e.surface.height == height &&
        e.surface.bitmap) {
      e.free = false;
      return e.surface;
    }
  }
  Rhi2dSurface created;
  if (!create_dib_section(hwnd, width, height, &created)) {
    return empty;
  }
  Entry e;
  e.surface = created;
  e.free = false;
  entries_.push_back(e);
  return created;
}

void Rhi2dSurfacePool::release(Rhi2dSurface surface) {
  if (!surface.bitmap) {
    return;
  }
  std::lock_guard<std::mutex> guard(lock_);
  for (Entry& e : entries_) {
    if (e.surface.bitmap == surface.bitmap) {
      // Keep generation / dirty metadata with the pooled bitmap.
      e.surface = surface;
      e.free = true;
      return;
    }
  }
  // Not from this pool (e.g. foreign share) — destroy.
  DeleteObject(surface.bitmap);
}

void Rhi2dSurfacePool::clear() {
  std::lock_guard<std::mutex> guard(lock_);
  for (Entry& e : entries_) {
    if (e.surface.bitmap) {
      DeleteObject(e.surface.bitmap);
      e.surface = Rhi2dSurface{};
    }
  }
  entries_.clear();
}

Rhi2dSurfacePool& rhi2d_surface_pool() {
  static Rhi2dSurfacePool pool;
  return pool;
}

}  // namespace detail
}  // namespace scenic
