// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/engine.h"

#include "scenic/render/detail/mem_frame.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace scenic {
namespace detail {
namespace {

int clamp_i(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

POINT map_to_view(const ViewXform& view, float mx, float my) {
  // Soft clamp only to keep GDI POINT in int32; do not pinch China rings
  // into a small box (hard ±30k clamps used to draw diagonal chords).
  constexpr double kLo = -1.0e7;
  constexpr double kHi = 1.0e7;
  const double x = static_cast<double>(mx) * view.scale + view.pan_x;
  const double y = static_cast<double>(my) * view.scale + view.pan_y;
  POINT p;
  p.x = static_cast<int>(
      std::lround(x < kLo ? kLo : (x > kHi ? kHi : x)));
  p.y = static_cast<int>(
      std::lround(y < kLo ? kLo : (y > kHi ? kHi : y)));
  return p;
}

uint64_t bits_u64(double v) {
  uint64_t u = 0;
  static_assert(sizeof(double) == sizeof(uint64_t), "double size");
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

uint64_t view_fingerprint(const ViewXform& view, uint32_t w, uint32_t h,
                          const DrawItem* items, uint32_t count) {
  uint64_t fp = 0;
  fp = mix_u64(fp, w);
  fp = mix_u64(fp, h);
  fp = mix_u64(fp, static_cast<uint64_t>(count));
  fp = mix_u64(fp, bits_u64(view.pan_x));
  fp = mix_u64(fp, bits_u64(view.pan_y));
  fp = mix_u64(fp, bits_u64(view.scale));
  fp = mix_u64(fp, reinterpret_cast<uintptr_t>(items));
  if (items && count > 0) {
    fp = mix_u64(fp, items[0].vertex_count);
    fp = mix_u64(fp, items[count - 1].vertex_count);
    if (items[0].xy) {
      fp = mix_u64(fp, static_cast<uint32_t>(items[0].xy[0].x * 1000.f));
      fp = mix_u64(fp, static_cast<uint32_t>(items[0].xy[0].y * 1000.f));
    }
  }
  return fp == 0 ? 1 : fp;
}

// Multipart rings often store parts in one vertex list. Connecting a part
// end to the next part start draws a long diagonal chord across China.
bool is_view_jump(const POINT& a, const POINT& b, int w, int h) {
  const int dx = a.x - b.x;
  const int dy = a.y - b.y;
  const int lim = (w < h ? w : h) / 2;
  const int lim2 = lim > 64 ? lim : 64;
  return (dx * dx + dy * dy) > (lim2 * lim2);
}

void flush_poly(HDC hdc, std::vector<POINT>* pts, bool fill_pass,
                COLORREF fill, COLORREF stroke, int pen_w) {
  if (!pts || pts->size() < 3) {
    pts->clear();
    return;
  }
  HPEN pen = CreatePen(PS_SOLID, pen_w, stroke ? stroke : RGB(40, 50, 60));
  HBRUSH brush = CreateSolidBrush(fill ? fill : RGB(196, 214, 160));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  HGDIOBJ old_brush = SelectObject(hdc, brush);
  if (fill_pass) {
    SetPolyFillMode(hdc, WINDING);
    Polygon(hdc, pts->data(), static_cast<int>(pts->size()));
  } else {
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Polyline(hdc, pts->data(), static_cast<int>(pts->size()));
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(brush);
  DeleteObject(pen);
  pts->clear();
}

void flush_line(HDC hdc, std::vector<POINT>* pts, COLORREF stroke, int pen_w) {
  if (!pts || pts->size() < 2) {
    pts->clear();
    return;
  }
  HPEN pen = CreatePen(PS_SOLID, pen_w, stroke ? stroke : RGB(40, 50, 60));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  SelectObject(hdc, GetStockObject(NULL_BRUSH));
  Polyline(hdc, pts->data(), static_cast<int>(pts->size()));
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
  pts->clear();
}

void paint_items(HDC hdc, uint32_t width_px, uint32_t height_px,
                 const ViewXform& view, const DrawItem* items, uint32_t count) {
  if (!hdc || width_px == 0 || height_px == 0) {
    return;
  }
  const int w = static_cast<int>(width_px);
  const int h = static_cast<int>(height_px);
  RECT full = {0, 0, w, h};
  HBRUSH ocean = CreateSolidBrush(RGB(170, 211, 223));
  FillRect(hdc, &full, ocean);
  DeleteObject(ocean);

  if (!items || count == 0) {
    return;
  }

  HRGN clip = CreateRectRgn(0, 0, w, h);
  if (clip) {
    SelectClipRgn(hdc, clip);
  }
  SetPolyFillMode(hdc, WINDING);

  std::vector<POINT> pts;
  pts.reserve(256);

  // Pass 0: polygon fills (no stroke) so land seams are not pen chords.
  // Pass 1: polygon outlines + lines + points.
  for (int pass = 0; pass < 2; ++pass) {
    for (uint32_t i = 0; i < count; ++i) {
      const DrawItem& it = items[i];
      if (!it.xy || it.vertex_count == 0) {
        continue;
      }
      const COLORREF fill = static_cast<COLORREF>(it.fill_colorref);
      const COLORREF stroke = static_cast<COLORREF>(it.stroke_colorref);
      const int pen_w = clamp_i(it.stroke_width_px, 1, 8);
      const uint32_t n = it.vertex_count;
      const uint32_t step = n > 2500u ? ((n + 2499u) / 2500u) : 1u;

      if (it.kind == GeomKind::kPolygon) {
        if (pass == 1) {
          // Outline only on second pass.
          pts.clear();
          POINT prev = {};
          bool have_prev = false;
          auto push_v = [&](uint32_t v) {
            POINT cur = map_to_view(view, it.xy[v].x, it.xy[v].y);
            if (have_prev && is_view_jump(prev, cur, w, h)) {
              flush_line(hdc, &pts, stroke, pen_w);
            }
            pts.push_back(cur);
            prev = cur;
            have_prev = true;
          };
          for (uint32_t v = 0; v < n; v += step) {
            push_v(v);
          }
          if (n >= 2 && step > 1 && (n - 1) % step != 0) {
            push_v(n - 1);
          }
          flush_line(hdc, &pts, stroke, pen_w);
          continue;
        }
        pts.clear();
        POINT prev = {};
        bool have_prev = false;
        auto push_v = [&](uint32_t v) {
          POINT cur = map_to_view(view, it.xy[v].x, it.xy[v].y);
          if (have_prev && is_view_jump(prev, cur, w, h)) {
            flush_poly(hdc, &pts, /*fill_pass=*/true, fill, stroke, pen_w);
          }
          pts.push_back(cur);
          prev = cur;
          have_prev = true;
        };
        for (uint32_t v = 0; v < n; v += step) {
          push_v(v);
        }
        if (n >= 2 && step > 1 && (n - 1) % step != 0) {
          push_v(n - 1);
        }
        flush_poly(hdc, &pts, /*fill_pass=*/true, fill, stroke, pen_w);
        continue;
      }

      if (pass == 0) {
        continue;
      }
      if (it.kind == GeomKind::kLine) {
        pts.clear();
        POINT prev = {};
        bool have_prev = false;
        for (uint32_t v = 0; v < n; v += step) {
          POINT cur = map_to_view(view, it.xy[v].x, it.xy[v].y);
          if (have_prev && is_view_jump(prev, cur, w, h)) {
            flush_line(hdc, &pts, stroke, pen_w);
          }
          pts.push_back(cur);
          prev = cur;
          have_prev = true;
        }
        if (n >= 2 && step > 1 && (n - 1) % step != 0) {
          POINT cur = map_to_view(view, it.xy[n - 1].x, it.xy[n - 1].y);
          if (have_prev && is_view_jump(prev, cur, w, h)) {
            flush_line(hdc, &pts, stroke, pen_w);
          }
          pts.push_back(cur);
        }
        flush_line(hdc, &pts, stroke, pen_w);
      } else {
        POINT p = map_to_view(view, it.xy[0].x, it.xy[0].y);
        const int r = it.kind == GeomKind::kText ? 3 : 4;
        HPEN pen = CreatePen(PS_SOLID, 1, stroke ? stroke : RGB(40, 50, 60));
        HBRUSH brush = CreateSolidBrush(fill ? fill : RGB(220, 200, 160));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        Ellipse(hdc, p.x - r, p.y - r, p.x + r, p.y + r);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(brush);
        DeleteObject(pen);
      }
    }
  }

  if (clip) {
    SelectClipRgn(hdc, nullptr);
    DeleteObject(clip);
  }
}

bool write_bmp_file(const char* path, int width_px, int height_px,
                    const void* bits, int stride_bytes) {
  if (!path || !bits || width_px <= 0 || height_px <= 0 || stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = -height_px;  // top-down DIB
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
                  std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
                  std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
}

}  // namespace

class Map2dEngine final : public Engine {
 public:
  bool initialize(const SessionDesc& desc) override {
    width_ = desc.width_px;
    height_ = desc.height_px;
    last_ok_ = width_ > 0 && height_ > 0;
    return last_ok_;
  }
  void shutdown() override { frame_.destroy(); }
  // HWND-free software present: rasterize into MemFrame so content
  // present_gpu / paint_hdc / export_bmp can succeed without FlyCube.
  bool present() override {
    if (width_ == 0 || height_ == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        view_fingerprint(view_, width_, height_, items_, count_);
    if (frame_.matches(width_, height_, fp)) {
      last_ok_ = true;
      return true;
    }
    if (!frame_.ensure(width_, height_)) {
      last_ok_ = false;
      return false;
    }
    paint_items(frame_.dc, width_, height_, view_, items_, count_);
    frame_.fingerprint = fp;
    last_ok_ = true;
    return true;
  }
  Kind kind() const override { return Kind::kMap2d; }

  void bind_view(const ViewXform& view) override { view_ = view; }
  void bind_draw_items(const DrawItem* items, uint32_t count) override {
    items_ = items;
    count_ = count;
  }

  bool paint_hdc(void* hdc, uint32_t width_px, uint32_t height_px) override {
    HDC dc = static_cast<HDC>(hdc);
    if (!dc || width_px == 0 || height_px == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        view_fingerprint(view_, width_px, height_px, items_, count_);
    if (frame_.matches(width_px, height_px, fp)) {
      last_ok_ = frame_.blit_to(dc);
      return last_ok_;
    }
    if (!frame_.ensure(width_px, height_px)) {
      paint_items(dc, width_px, height_px, view_, items_, count_);
      last_ok_ = true;
      return true;
    }
    paint_items(frame_.dc, width_px, height_px, view_, items_, count_);
    frame_.fingerprint = fp;
    last_ok_ = frame_.blit_to(dc);
    return last_ok_;
  }

  bool export_bmp(const char* path, uint32_t width_px,
                  uint32_t height_px) override {
    if (!path || width_px == 0 || height_px == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        view_fingerprint(view_, width_px, height_px, items_, count_);
    if (!frame_.matches(width_px, height_px, fp)) {
      if (!frame_.ensure(width_px, height_px)) {
        last_ok_ = false;
        return false;
      }
      paint_items(frame_.dc, width_px, height_px, view_, items_, count_);
      frame_.fingerprint = fp;
    }
    const int stride = static_cast<int>(width_px) * 4;
    last_ok_ = write_bmp_file(path, static_cast<int>(width_px),
                              static_cast<int>(height_px), frame_.bits, stride);
    return last_ok_;
  }

  bool last_present_ok() const override { return last_ok_; }

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  ViewXform view_{};
  const DrawItem* items_ = nullptr;
  uint32_t count_ = 0;
  bool last_ok_ = false;
  MemFrame frame_;
};

}  // namespace detail

Engine* create_map2d_engine() { return new detail::Map2dEngine(); }

}  // namespace scenic
