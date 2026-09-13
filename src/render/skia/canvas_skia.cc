// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Real Skia backend for render::skia::Canvas (phase D). Compiled only when
// smt_has_skia=true and the local pin is present. Same public API as canvas.cc.
// Targets a modern Skia pin (SkSurfaces::WrapPixels, DirectWrite FontMgr).

#include "render/skia/canvas.h"

// windows.h (via canvas.h) must not define min/max macros before Skia headers.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#if !__has_include("include/core/SkCanvas.h")
#error \
    "SMT_HAS_SKIA requires a local Skia pin at third_party/.src/skia " \
    "(junction/symlink). See src/render/skia/README.md — do not GitHub-clone into git."
#endif

#include <cstdint>
#include <cstring>

#include "include/core/SkCanvas.h"
#include "include/core/SkClipOp.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkTypeface_win.h"

namespace render {
namespace skia {
namespace {

SkColor to_sk_color(Color color) {
  const std::uint8_t a = static_cast<std::uint8_t>((color >> 24) & 0xFF);
  const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFF);
  const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFF);
  const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFF);
  return SkColorSetARGB(a, r, g, b);
}

SkFont make_ui_font() {
  sk_sp<SkFontMgr> mgr = SkFontMgr_New_DirectWrite();
  sk_sp<SkTypeface> face;
  if (mgr) {
    face = mgr->legacyMakeTypeface("Segoe UI", SkFontStyle::Normal());
    if (!face) {
      face = mgr->legacyMakeTypeface(nullptr, SkFontStyle::Normal());
    }
  }
  // Size ~ GDI DEFAULT_GUI_FONT; measure tests only require non-zero ink.
  return SkFont(std::move(face), 12.0f);
}

}  // namespace

struct Canvas::SkiaState {
  HBITMAP dib = nullptr;
  HDC mem_dc = nullptr;
  void* pixels = nullptr;
  int stride = 0;
  sk_sp<SkSurface> surface;
  SkCanvas* canvas = nullptr;
  SkFont font;
  bool ready = false;

  bool init(int width, int height) {
    if (width <= 0 || height <= 0) {
      return false;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
      return false;
    }
    pixels = bits;
    stride = width * 4;
    std::memset(bits, 0,
                static_cast<size_t>(stride) * static_cast<size_t>(height));

    mem_dc = CreateCompatibleDC(nullptr);
    if (!mem_dc) {
      return false;
    }
    SelectObject(mem_dc, dib);

    const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    surface = SkSurfaces::WrapPixels(info, pixels, static_cast<size_t>(stride));
    if (!surface) {
      return false;
    }
    canvas = surface->getCanvas();
    if (!canvas) {
      return false;
    }
    font = make_ui_font();
    ready = true;
    return true;
  }

  void destroy() {
    canvas = nullptr;
    surface.reset();
    if (mem_dc) {
      DeleteDC(mem_dc);
      mem_dc = nullptr;
    }
    if (dib) {
      DeleteObject(dib);
      dib = nullptr;
    }
    pixels = nullptr;
    ready = false;
  }
};

Canvas::Canvas(HDC hdc, int width, int height)
    : hdc_(hdc), width_(width), height_(height), skia_(new SkiaState) {
  if (!skia_->init(width, height)) {
    // Initialize failed: keep a live object that no-ops. See README for pin /
    // matching Windows skia.lib requirements.
    skia_->destroy();
  }
}

Canvas::~Canvas() {
  if (skia_) {
    skia_->destroy();
    delete skia_;
    skia_ = nullptr;
  }
}

void Canvas::fill_rect(int x, int y, int w, int h, Color color) {
  if (!skia_ || !skia_->ready || !skia_->canvas || w <= 0 || h <= 0) {
    return;
  }
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(to_sk_color(color));
  skia_->canvas->drawRect(SkRect::MakeXYWH(static_cast<SkScalar>(x),
                                           static_cast<SkScalar>(y),
                                           static_cast<SkScalar>(w),
                                           static_cast<SkScalar>(h)),
                          paint);
  if (hdc_ && skia_->mem_dc) {
    BitBlt(hdc_, x, y, w, h, skia_->mem_dc, x, y, SRCCOPY);
  }
}

void Canvas::stroke_rect(int x, int y, int w, int h, Color color,
                         int stroke_width) {
  if (!skia_ || !skia_->ready || !skia_->canvas || w <= 0 || h <= 0) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(static_cast<SkScalar>(stroke_width));
  paint.setColor(to_sk_color(color));
  const SkScalar inset = stroke_width * 0.5f;
  const SkRect rect = SkRect::MakeXYWH(
      static_cast<SkScalar>(x) + inset, static_cast<SkScalar>(y) + inset,
      static_cast<SkScalar>(w - stroke_width),
      static_cast<SkScalar>(h - stroke_width));
  skia_->canvas->drawRect(rect, paint);
  if (hdc_ && skia_->mem_dc) {
    BitBlt(hdc_, x, y, w, h, skia_->mem_dc, x, y, SRCCOPY);
  }
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color,
                       int stroke_width) {
  if (!skia_ || !skia_->ready || !skia_->canvas) {
    return;
  }
  if (stroke_width < 1) {
    stroke_width = 1;
  }
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(static_cast<SkScalar>(stroke_width));
  paint.setColor(to_sk_color(color));
  skia_->canvas->drawLine(static_cast<SkScalar>(x0), static_cast<SkScalar>(y0),
                          static_cast<SkScalar>(x1), static_cast<SkScalar>(y1),
                          paint);
  if (hdc_ && skia_->mem_dc) {
    const int left = (x0 < x1) ? x0 : x1;
    const int top = (y0 < y1) ? y0 : y1;
    const int right = (x0 > x1) ? x0 : x1;
    const int bottom = (y0 > y1) ? y0 : y1;
    const int pad = stroke_width + 1;
    BitBlt(hdc_, left - pad, top - pad, (right - left) + 2 * pad,
           (bottom - top) + 2 * pad, skia_->mem_dc, left - pad, top - pad,
           SRCCOPY);
  }
}

void Canvas::draw_text(int x, int y, const wchar_t* text, Color color) {
  if (!skia_ || !skia_->ready || !skia_->canvas || !text || !*text) {
    return;
  }
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(to_sk_color(color));

  SkFontMetrics metrics = {};
  skia_->font.getMetrics(&metrics);
  // GDI TextOut y is the top of the cell; Skia y is the baseline.
  const SkScalar baseline = static_cast<SkScalar>(y) - metrics.fAscent;
  const size_t len = static_cast<size_t>(lstrlenW(text));
  skia_->canvas->drawSimpleText(text, len * sizeof(wchar_t),
                                SkTextEncoding::kUTF16,
                                static_cast<SkScalar>(x), baseline, skia_->font,
                                paint);

  if (hdc_ && skia_->mem_dc) {
    const SkScalar width = skia_->font.measureText(
        text, len * sizeof(wchar_t), SkTextEncoding::kUTF16);
    const int ink_w = static_cast<int>(width) + 2;
    const int ink_h =
        static_cast<int>(metrics.fDescent - metrics.fAscent) + 2;
    BitBlt(hdc_, x, y, ink_w, ink_h, skia_->mem_dc, x, y, SRCCOPY);
  }
}

Size Canvas::measure_text(const wchar_t* text) const {
  Size out;
  if (!skia_ || !skia_->ready || !text || !*text) {
    return out;
  }
  const size_t len = static_cast<size_t>(lstrlenW(text));
  const SkScalar width = skia_->font.measureText(
      text, len * sizeof(wchar_t), SkTextEncoding::kUTF16);
  SkFontMetrics metrics = {};
  skia_->font.getMetrics(&metrics);
  out.width = static_cast<int>(width + 0.5f);
  out.height = static_cast<int>(metrics.fDescent - metrics.fAscent + 0.5f);
  if (out.width < 0) {
    out.width = 0;
  }
  if (out.height < 0) {
    out.height = 0;
  }
  return out;
}

void Canvas::clip_rect(int x, int y, int w, int h) {
  if (!skia_ || !skia_->ready || !skia_->canvas || w <= 0 || h <= 0) {
    return;
  }
  skia_->canvas->clipRect(
      SkRect::MakeXYWH(static_cast<SkScalar>(x), static_cast<SkScalar>(y),
                       static_cast<SkScalar>(w), static_cast<SkScalar>(h)),
      SkClipOp::kIntersect, false);
}

void Canvas::save() {
  if (!skia_ || !skia_->ready || !skia_->canvas) {
    return;
  }
  skia_->canvas->save();
}

void Canvas::restore() {
  if (!skia_ || !skia_->ready || !skia_->canvas) {
    return;
  }
  if (skia_->canvas->getSaveCount() > 1) {
    skia_->canvas->restore();
  }
}

}  // namespace skia
}  // namespace render
