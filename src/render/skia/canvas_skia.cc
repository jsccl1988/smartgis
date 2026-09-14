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

// Skia value types stay off Canvas::Backend's default ctor/dtor so a failed
// or unused backend cannot run mismatched SkFont/sk_sp ABI during chrome paint.
struct SkiaRaster {
  sk_sp<SkSurface> surface;
  SkFont font;
};

}  // namespace

struct Canvas::Backend {
  HBITMAP dib = nullptr;
  HBITMAP old_dib = nullptr;
  HDC mem_dc = nullptr;
  void* pixels = nullptr;
  int stride = 0;
  SkiaRaster* raster = nullptr;
  SkCanvas* canvas = nullptr;
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
      destroy();
      return false;
    }
    old_dib = static_cast<HBITMAP>(SelectObject(mem_dc, dib));

    raster = new SkiaRaster();
    const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    raster->surface =
        SkSurfaces::WrapPixels(info, pixels, static_cast<size_t>(stride));
    if (!raster->surface) {
      destroy();
      return false;
    }
    canvas = raster->surface->getCanvas();
    if (!canvas) {
      destroy();
      return false;
    }
    raster->font = make_ui_font();
    ready = true;
    return true;
  }

  void destroy() {
    canvas = nullptr;
    delete raster;
    raster = nullptr;
    if (mem_dc) {
      // Must deselect the DIB before DeleteObject / DeleteDC or GDI can
      // corrupt the process heap and AV later during chrome teardown.
      if (old_dib) {
        SelectObject(mem_dc, old_dib);
        old_dib = nullptr;
      }
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
    : hdc_(hdc), width_(width), height_(height), backend_(new Backend) {
  if (!backend_->init(width, height)) {
    // Initialize failed: keep a live object that no-ops. See README for pin /
    // matching Windows skia.lib requirements.
    backend_->destroy();
  }
}

Canvas::~Canvas() {
  if (backend_) {
    backend_->destroy();
    delete backend_;
    backend_ = nullptr;
  }
}

void Canvas::fill_rect(int x, int y, int w, int h, Color color) {
  if (!backend_ || !backend_->ready || !backend_->canvas || w <= 0 || h <= 0) {
    return;
  }
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(to_sk_color(color));
  backend_->canvas->drawRect(SkRect::MakeXYWH(static_cast<SkScalar>(x),
                                              static_cast<SkScalar>(y),
                                              static_cast<SkScalar>(w),
                                              static_cast<SkScalar>(h)),
                             paint);
  if (hdc_ && backend_->mem_dc) {
    BitBlt(hdc_, x, y, w, h, backend_->mem_dc, x, y, SRCCOPY);
  }
}

void Canvas::stroke_rect(int x, int y, int w, int h, Color color,
                         int stroke_width) {
  if (!backend_ || !backend_->ready || !backend_->canvas || w <= 0 || h <= 0) {
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
  backend_->canvas->drawRect(rect, paint);
  if (hdc_ && backend_->mem_dc) {
    BitBlt(hdc_, x, y, w, h, backend_->mem_dc, x, y, SRCCOPY);
  }
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color,
                       int stroke_width) {
  if (!backend_ || !backend_->ready || !backend_->canvas) {
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
  backend_->canvas->drawLine(static_cast<SkScalar>(x0),
                             static_cast<SkScalar>(y0),
                             static_cast<SkScalar>(x1),
                             static_cast<SkScalar>(y1), paint);
  if (hdc_ && backend_->mem_dc) {
    const int left = (x0 < x1) ? x0 : x1;
    const int top = (y0 < y1) ? y0 : y1;
    const int right = (x0 > x1) ? x0 : x1;
    const int bottom = (y0 > y1) ? y0 : y1;
    const int pad = stroke_width + 1;
    BitBlt(hdc_, left - pad, top - pad, (right - left) + 2 * pad,
           (bottom - top) + 2 * pad, backend_->mem_dc, left - pad, top - pad,
           SRCCOPY);
  }
}

void Canvas::draw_text(int x, int y, const wchar_t* text, Color color) {
  if (!backend_ || !backend_->ready || !backend_->canvas || !backend_->raster ||
      !text || !*text) {
    return;
  }
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(to_sk_color(color));

  SkFontMetrics metrics = {};
  backend_->raster->font.getMetrics(&metrics);
  // GDI TextOut y is the top of the cell; Skia y is the baseline.
  const SkScalar baseline = static_cast<SkScalar>(y) - metrics.fAscent;
  const size_t len = static_cast<size_t>(lstrlenW(text));
  backend_->canvas->drawSimpleText(text, len * sizeof(wchar_t),
                                   SkTextEncoding::kUTF16,
                                   static_cast<SkScalar>(x), baseline,
                                   backend_->raster->font, paint);

  if (hdc_ && backend_->mem_dc) {
    const SkScalar width = backend_->raster->font.measureText(
        text, len * sizeof(wchar_t), SkTextEncoding::kUTF16);
    const int ink_w = static_cast<int>(width) + 2;
    const int ink_h =
        static_cast<int>(metrics.fDescent - metrics.fAscent) + 2;
    BitBlt(hdc_, x, y, ink_w, ink_h, backend_->mem_dc, x, y, SRCCOPY);
  }
}

Size Canvas::measure_text(const wchar_t* text) const {
  Size out;
  if (!backend_ || !backend_->ready || !backend_->raster || !text || !*text) {
    return out;
  }
  const size_t len = static_cast<size_t>(lstrlenW(text));
  const SkScalar width = backend_->raster->font.measureText(
      text, len * sizeof(wchar_t), SkTextEncoding::kUTF16);
  SkFontMetrics metrics = {};
  backend_->raster->font.getMetrics(&metrics);
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
  if (!backend_ || !backend_->ready || !backend_->canvas || w <= 0 || h <= 0) {
    return;
  }
  backend_->canvas->clipRect(
      SkRect::MakeXYWH(static_cast<SkScalar>(x), static_cast<SkScalar>(y),
                       static_cast<SkScalar>(w), static_cast<SkScalar>(h)),
      SkClipOp::kIntersect, false);
}

void Canvas::save() {
  if (!backend_ || !backend_->ready || !backend_->canvas) {
    return;
  }
  backend_->canvas->save();
}

void Canvas::restore() {
  if (!backend_ || !backend_->ready || !backend_->canvas) {
    return;
  }
  if (backend_->canvas->getSaveCount() > 1) {
    backend_->canvas->restore();
  }
}

}  // namespace skia
}  // namespace render
