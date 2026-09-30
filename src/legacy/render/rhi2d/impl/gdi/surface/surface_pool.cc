// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"

#include <algorithm>
#include <cstring>

#include "legacy/render/rhi2d/impl/gdi/surface/compose.h"

#pragma comment(lib, "Msimg32.lib")

namespace render {
namespace {

bool create_dib_section(HWND hwnd, int width, int height, GdiSurface* out) {
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

GdiSurfacePool::~GdiSurfacePool() { clear(); }

GdiSurface GdiSurfacePool::acquire(HWND hwnd, int width, int height) {
  GdiSurface empty;
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
  GdiSurface created;
  if (!create_dib_section(hwnd, width, height, &created)) {
    return empty;
  }
  Entry e;
  e.surface = created;
  e.free = false;
  entries_.push_back(e);
  return created;
}

void GdiSurfacePool::release(GdiSurface surface) {
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
  // Not from this pool (e.g. foreign share) �?destroy.
  DeleteObject(surface.bitmap);
}

void GdiSurfacePool::clear() {
  std::lock_guard<std::mutex> guard(lock_);
  for (Entry& e : entries_) {
    if (e.surface.bitmap) {
      DeleteObject(e.surface.bitmap);
      e.surface = GdiSurface{};
    }
  }
  entries_.clear();
}

GdiSurfacePool& gdi_surface_pool() {
  static GdiSurfacePool pool;
  return pool;
}

GdiOwnedSurface::GdiOwnedSurface(HWND hwnd) : hwnd_(hwnd) {}

GdiOwnedSurface::~GdiOwnedSurface() {
  end_dc();
  release_owned();
}

void GdiOwnedSurface::release_owned() {
  if (!owned_ || !surface_.bitmap) {
    surface_ = GdiSurface{};
    owned_ = true;
    return;
  }
  gdi_surface_pool().release(surface_);
  surface_ = GdiSurface{};
  owned_ = true;
}

GdiOwnedSurface& GdiOwnedSurface::assign_size_from(
    const GdiOwnedSurface& other) {
  if (&other == this) {
    return *this;
  }
  release_owned();
  owned_ = true;
  set_wnd(other.wnd());
  set_size(other.width(), other.height());
  return *this;
}

long GdiOwnedSurface::set_wnd(HWND hwnd) {
  if (!owned_) {
    return SMT_ERR_FAILURE;
  }
#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  hwnd_ = hwnd;
  return SMT_ERR_NONE;
}

long GdiOwnedSurface::set_size(int cx, int cy) {
  if (!owned_ || cx < 1 || cy < 1) {
    return SMT_ERR_FAILURE;
  }

  end_dc();

  // Same footprint: keep the DIB (pool-friendly reuse across frames).
  if (surface_.bitmap && surface_.bits && surface_.width == cx &&
      surface_.height == cy) {
    return clear(0, 0, cx, cy);
  }

#ifdef SMT_THREAD_SAFE
  lock_.lock();
#endif
  release_owned();
  surface_ = gdi_surface_pool().acquire(hwnd_, cx, cy);
  owned_ = true;
  const bool ok = surface_.bitmap != nullptr && surface_.bits != nullptr;
#ifdef SMT_THREAD_SAFE
  lock_.unlock();
#endif
  // clear() may take the same lock; do not call while held.
  if (!ok) {
    return SMT_ERR_FAILURE;
  }
  return clear(0, 0, surface_.width, surface_.height);
}

long GdiOwnedSurface::share_from(const GdiOwnedSurface& src) {
  end_dc();
  release_owned();

#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  hwnd_ = src.wnd();
  surface_ = src.surface();
  owned_ = false;
  return SMT_ERR_NONE;
}

long GdiOwnedSurface::clear(int x, int y, int w, int h, COLORREF clr) {
  if (w < 1 || h < 1) {
    return SMT_ERR_FAILURE;
  }

#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  if (surface_.bits && x >= 0 && y >= 0 && x + w <= surface_.width &&
      y + h <= surface_.height) {
    const uint8_t b = GetBValue(clr);
    const uint8_t g = GetGValue(clr);
    const uint8_t r = GetRValue(clr);
    // BGRA dword fill �?same solid-clear idea as software map2d underlay.
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
    return SMT_ERR_NONE;
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
  return SMT_ERR_NONE;
}

long GdiOwnedSurface::present(int dest_org_x, int dest_org_y, int dest_w,
                              int dest_h, int src_org_x, int src_org_y,
                              int op) {
  // 1:1 HWND present �?reuse the stretch overload with equal src/dest size.
  return present(dest_org_x, dest_org_y, dest_w, dest_h, src_org_x, src_org_y,
                 dest_w, dest_h, BLT_STRETCH, op, RGB(255, 255, 255));
}

long GdiOwnedSurface::present(int dest_org_x, int dest_org_y, int dest_w,
                              int dest_h, int src_org_x, int src_org_y,
                              int src_w, int src_h, eSwapType type, int op,
                              COLORREF clr) {
  if (surface_.bitmap == nullptr) {
    return SMT_ERR_FAILURE;
  }

#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  HDC hdc = GetDC(hwnd_);
  HDC src_dc = prepare_dc(false);
  BOOL ok = TRUE;
  switch (type) {
    case BLT_STRETCH:
      ok = ::StretchBlt(hdc, dest_org_x, dest_org_y, dest_w, dest_h, src_dc,
                        src_org_x, src_org_y, src_w, src_h, op);
      break;
    case BLT_TRANSPARENT:
      // HWND present stays GDI TransparentBlt (color-key). Layer compose uses
      // blend_render_pass in blit_to.
      ok = ::TransparentBlt(hdc, dest_org_x, dest_org_y, dest_w, dest_h, src_dc,
                            src_org_x, src_org_y, src_w, src_h, clr);
      break;
  }
  end_dc();
  ::ReleaseDC(hwnd_, hdc);
  return ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long GdiOwnedSurface::blit_to(GdiOwnedSurface& target, int dest_org_x,
                              int dest_org_y, int dest_w, int dest_h,
                              int src_org_x, int src_org_y, int op) {
  if (surface_.bitmap == nullptr) {
    return SMT_ERR_FAILURE;
  }

  // Opaque 1:1 copy: GDI BitBlt. Soft compose here only burned CPU and
  // diverged from the pre-strangler present path (look regression).
#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  HDC target_dc = target.prepare_dc(false);
  HDC src_dc = prepare_dc(false);
  const BOOL ok = ::BitBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                           src_dc, src_org_x, src_org_y, op);
  end_dc();
  target.end_dc();
  if (!ok) {
    return SMT_ERR_FAILURE;
  }
  target.surface().bump_generation();
  target.surface().mark_dirty(dest_org_x, dest_org_y, dest_w, dest_h);
  return SMT_ERR_NONE;
}

long GdiOwnedSurface::blit_to(GdiOwnedSurface& target, int dest_org_x,
                              int dest_org_y, int dest_w, int dest_h,
                              int src_org_x, int src_org_y, int src_w,
                              int src_h, eSwapType type, int op, COLORREF clr) {
  if (surface_.bitmap == nullptr) {
    return SMT_ERR_FAILURE;
  }

  const bool need_scale = (src_w != dest_w) || (src_h != dest_h);
  const bool color_key = (type == BLT_TRANSPARENT);
  // Soft compose is NN stretch + multi-buffer temps. Map Refresh/publish
  // used it for every StretchBlt �?worse carto + wheel-settle OOM/exit.
  // Keep CPU compose only for 1:1 color-key overlays; stretch/opaque �?GDI.
  if (color_key && !need_scale &&
      detail::compose_buf_to_buf(surface_, target.surface(), dest_org_x,
                                 dest_org_y, dest_w, dest_h, src_org_x,
                                 src_org_y, src_w, src_h, /*stretch=*/false,
                                 /*color_key=*/true, clr)) {
    // compose_buf_to_buf already bumps generation + marks dirty on target.
    return SMT_ERR_NONE;
  }

#ifdef SMT_THREAD_SAFE
  std::lock_guard<std::mutex> guard(lock_);
#endif
  HDC target_dc = target.prepare_dc(false);
  HDC src_dc = prepare_dc(false);
  BOOL ok = TRUE;
  switch (type) {
    case BLT_STRETCH:
      ok = ::StretchBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                        src_dc, src_org_x, src_org_y, src_w, src_h, op);
      break;
    case BLT_TRANSPARENT:
      ok = ::TransparentBlt(target_dc, dest_org_x, dest_org_y, dest_w, dest_h,
                            src_dc, src_org_x, src_org_y, src_w, src_h, clr);
      break;
  }
  end_dc();
  target.end_dc();
  if (!ok) {
    return SMT_ERR_FAILURE;
  }
  target.surface().bump_generation();
  target.surface().mark_dirty(dest_org_x, dest_org_y, dest_w, dest_h);
  return SMT_ERR_NONE;
}

HDC GdiOwnedSurface::prepare_dc(bool clip) {
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

long GdiOwnedSurface::end_dc() {
  if (paint_dc_) {
    SelectObject(paint_dc_, old_bitmap_);
    DeleteDC(paint_dc_);
    paint_dc_ = nullptr;
    old_bitmap_ = nullptr;
  }
  return SMT_ERR_NONE;
}

}  // namespace render
