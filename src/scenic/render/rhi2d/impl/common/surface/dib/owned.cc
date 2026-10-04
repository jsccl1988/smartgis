// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

#include <algorithm>
#include <cstring>

namespace scenic {
namespace detail {

Rhi2dOwnedSurface::Rhi2dOwnedSurface(HWND hwnd) : hwnd_(hwnd) {}

Rhi2dOwnedSurface::~Rhi2dOwnedSurface() {
  end_dc();
  release_owned();
}

void Rhi2dOwnedSurface::release_owned() {
  if (!owned_ || !surface_.bitmap) {
    surface_ = Rhi2dSurface{};
    owned_ = true;
    return;
  }
  rhi2d_surface_pool().release(surface_);
  surface_ = Rhi2dSurface{};
  owned_ = true;
}

Rhi2dOwnedSurface& Rhi2dOwnedSurface::assign_size_from(
    const Rhi2dOwnedSurface& other) {
  if (&other == this) {
    return *this;
  }
  release_owned();
  owned_ = true;
  set_wnd(other.wnd());
  set_size(other.width(), other.height());
  return *this;
}

long Rhi2dOwnedSurface::set_wnd(HWND hwnd) {
  if (!owned_) {
    return kErrFailure;
  }
  std::lock_guard<std::mutex> guard(lock_);
  hwnd_ = hwnd;
  return kErrNone;
}

long Rhi2dOwnedSurface::set_size(int cx, int cy) {
  if (!owned_ || cx < 1 || cy < 1) {
    return kErrFailure;
  }

  end_dc();

  // Same footprint: keep the DIB (pool-friendly reuse across frames).
  if (surface_.bitmap && surface_.bits && surface_.width == cx &&
      surface_.height == cy) {
    return clear(0, 0, cx, cy);
  }

  lock_.lock();
  release_owned();
  surface_ = rhi2d_surface_pool().acquire(hwnd_, cx, cy);
  owned_ = true;
  const bool ok = surface_.bitmap != nullptr && surface_.bits != nullptr;
  lock_.unlock();
  // clear() may take the same lock; do not call while held.
  if (!ok) {
    return kErrFailure;
  }
  return clear(0, 0, surface_.width, surface_.height);
}

long Rhi2dOwnedSurface::share_from(const Rhi2dOwnedSurface& src) {
  end_dc();
  release_owned();

  std::lock_guard<std::mutex> guard(lock_);
  hwnd_ = src.wnd();
  surface_ = src.surface();
  owned_ = false;
  return kErrNone;
}

long Rhi2dOwnedSurface::clear(int x, int y, int w, int h, COLORREF clr) {
  if (w < 1 || h < 1) {
    return kErrFailure;
  }

  std::lock_guard<std::mutex> guard(lock_);
  if (surface_.bits && x >= 0 && y >= 0 && x + w <= surface_.width &&
      y + h <= surface_.height) {
    const uint8_t b = GetBValue(clr);
    const uint8_t g = GetGValue(clr);
    const uint8_t r = GetRValue(clr);
    // BGRA dword fill — same solid-clear idea as software map2d underlay.
    const uint32_t packed = static_cast<uint32_t>(b) |
                            (static_cast<uint32_t>(g) << 8) |
                            (static_cast<uint32_t>(r) << 16) | (255u << 24);
    auto* bits = static_cast<uint8_t*>(surface_.bits);
    if (x == 0 && y == 0 && w == surface_.width && h == surface_.height &&
        surface_.stride_bytes == static_cast<uint32_t>(w) * 4u) {
      auto* px = reinterpret_cast<uint32_t*>(bits);
      std::fill_n(px, static_cast<size_t>(w) * static_cast<size_t>(h), packed);
    } else {
      for (int row = 0; row < h; ++row) {
        auto* px = reinterpret_cast<uint32_t*>(
            bits + static_cast<size_t>(y + row) * surface_.stride_bytes +
            static_cast<size_t>(x) * 4u);
        std::fill_n(px, static_cast<size_t>(w), packed);
      }
    }
    surface_.bump_generation();
    surface_.mark_dirty(x, y, w, h);
    return kErrNone;
  }

  // Fallback GDI fill when bits are unavailable (shared foreign bitmap).
  RECT rect;
  rect.left = x;
  rect.top = y;
  rect.right = x + w;
  rect.bottom = y + h;

  HDC hdc = GetDC(hwnd_);
  HDC paint_dc = CreateCompatibleDC(hdc);
  HBITMAP prev = (HBITMAP)::SelectObject(paint_dc, surface_.bitmap);

  HBRUSH brush = CreateSolidBrush(clr);
  HBRUSH old_brush = (HBRUSH)::SelectObject(paint_dc, brush);
  ::FillRect(paint_dc, &rect, brush);
  ::SelectObject(paint_dc, old_brush);
  ::DeleteObject(brush);

  ::SelectObject(paint_dc, prev);
  ::DeleteDC(paint_dc);
  ::ReleaseDC(hwnd_, hdc);
  surface_.bump_generation();
  surface_.mark_dirty(x, y, w, h);
  return kErrNone;
}

HDC Rhi2dOwnedSurface::prepare_dc(bool clip) {
  if (paint_dc_) {
    end_dc();
  }
  // Offscreen DIB: CreateCompatibleDC(nullptr) avoids GetDC(HWND), which can
  // hang when multiple FrameJob workers paint layer surfaces in parallel.
  paint_dc_ = ::CreateCompatibleDC(nullptr);
  if (!paint_dc_) {
    return nullptr;
  }
  old_bitmap_ = (HBITMAP)SelectObject(paint_dc_, surface_.bitmap);

  if (clip && surface_.width > 0 && surface_.height > 0) {
    HRGN rgn = ::CreateRectRgn(0, 0, surface_.width, surface_.height);
    ::SelectClipRgn(paint_dc_, rgn);
    ::DeleteObject(rgn);
  }
  return paint_dc_;
}

long Rhi2dOwnedSurface::end_dc() {
  if (paint_dc_) {
    SelectObject(paint_dc_, old_bitmap_);
    DeleteDC(paint_dc_);
    paint_dc_ = nullptr;
    old_bitmap_ = nullptr;
  }
  return kErrNone;
}

}  // namespace detail
}  // namespace scenic
