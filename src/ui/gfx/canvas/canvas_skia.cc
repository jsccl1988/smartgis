// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Real Skia backend for ui::gfx::Canvas. Compiled when has_skia=true and
// the local pin is present. Same paint ops as canvas_gdi.cc via CanvasBackend.

#include "ui/gfx/canvas/canvas_backend.h"

#include "ui/gfx/raster/paint_stats.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#if !__has_include("include/core/SkCanvas.h")
#error \
    "HAS_SKIA requires a local Skia pin at third_party/.src/skia " \
    "(junction/symlink). See src/ui/gfx/README.md — do not GitHub-clone into git."
#endif

#include <cstdint>
#include <cstring>
#include <vector>

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

namespace ui {
namespace gfx {
namespace detail {
namespace {

SkColor to_sk_color(Color color) {
  const std::uint8_t a = static_cast<std::uint8_t>((color >> 24) & 0xFF);
  const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFF);
  const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFF);
  const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFF);
  return SkColorSetARGB(a, r, g, b);
}

sk_sp<SkTypeface> ui_typeface() {
  static sk_sp<SkTypeface> face;
  static bool tried = false;
  if (!tried) {
    tried = true;
    sk_sp<SkFontMgr> mgr = SkFontMgr_New_DirectWrite();
    if (mgr) {
      face = mgr->legacyMakeTypeface("Segoe UI", SkFontStyle::Normal());
      if (!face) {
        face = mgr->legacyMakeTypeface(nullptr, SkFontStyle::Normal());
      }
    }
  }
  return face;
}

SkFont font_for_px(float px) {
  if (px < 1.f) {
    px = 12.f;
  }
  const int key = static_cast<int>(px + 0.5f);
  struct Slot {
    int key;
    SkFont font;
  };
  static std::vector<Slot> cache;
  for (const Slot& slot : cache) {
    if (slot.key == key) {
      return slot.font;
    }
  }
  SkFont font(ui_typeface(), static_cast<SkScalar>(key));
  if (cache.size() < 8) {
    cache.push_back(Slot{key, font});
  }
  return font;
}

float font_px_from_dc(HDC hdc) {
  if (!hdc) {
    return 12.f;
  }
  TEXTMETRICW tm = {};
  if (!GetTextMetricsW(hdc, &tm)) {
    return 12.f;
  }
  const int px = tm.tmHeight - tm.tmInternalLeading;
  return px > 0 ? static_cast<float>(px) : 12.f;
}

struct SkiaRaster {
  sk_sp<SkSurface> surface;
  SkFont font;
};

struct RetainedSurface {
  void* pixels = nullptr;
  int width = 0;
  int height = 0;
  int stride = 0;
  SkiaRaster* raster = nullptr;
  SkCanvas* canvas = nullptr;
};

RetainedSurface g_retained;

void release_retained() {
  delete g_retained.raster;
  g_retained = {};
}

bool wrap_retained(void* bits, int width, int height, int stride, float font_px,
                   SkiaRaster** raster_out, SkCanvas** canvas_out) {
  if (!bits || width <= 0 || height <= 0 || stride <= 0) {
    return false;
  }
  if (g_retained.raster && g_retained.pixels == bits &&
      g_retained.width == width && g_retained.height == height &&
      g_retained.stride == stride) {
    g_retained.raster->font = font_for_px(font_px);
    *raster_out = g_retained.raster;
    *canvas_out = g_retained.canvas;
    return g_retained.canvas != nullptr;
  }
  release_retained();
  auto* raster = new SkiaRaster();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  raster->surface =
      SkSurfaces::WrapPixels(info, bits, static_cast<size_t>(stride));
  if (!raster->surface) {
    delete raster;
    return false;
  }
  SkCanvas* canvas = raster->surface->getCanvas();
  if (!canvas) {
    delete raster;
    return false;
  }
  raster->font = font_for_px(font_px);
  g_retained.pixels = bits;
  g_retained.width = width;
  g_retained.height = height;
  g_retained.stride = stride;
  g_retained.raster = raster;
  g_retained.canvas = canvas;
  *raster_out = raster;
  *canvas_out = canvas;
  return true;
}

class SkiaCanvasBackend final : public CanvasBackend {
 public:
  SkiaCanvasBackend(HDC hdc, int width, int height)
      : hdc_(hdc), width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
      return;
    }
    const float font_px = font_px_from_dc(hdc);
    HGDIOBJ obj = hdc ? GetCurrentObject(hdc, OBJ_BITMAP) : nullptr;
    DIBSECTION section = {};
    if (obj && GetObject(obj, sizeof(section), &section) == sizeof(section) &&
        section.dsBm.bmBits && section.dsBm.bmWidth > 0 &&
        section.dsBm.bmHeight != 0) {
      const int dib_h = section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight
                                                   : section.dsBm.bmHeight;
      const int dib_w = section.dsBm.bmWidth;
      if (wrap_retained(section.dsBm.bmBits, dib_w, dib_h,
                        section.dsBm.bmWidthBytes, font_px, &raster_,
                        &canvas_)) {
        pixels_ = section.dsBm.bmBits;
        stride_ = section.dsBm.bmWidthBytes;
        owns_dib_ = false;
        ready_ = true;
        return;
      }
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    dib_ = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib_ || !bits) {
      if (dib_) {
        DeleteObject(dib_);
        dib_ = nullptr;
      }
      return;
    }
    pixels_ = bits;
    stride_ = width * 4;
    std::memset(bits, 0, static_cast<size_t>(stride_) * static_cast<size_t>(height));
    mem_dc_ = CreateCompatibleDC(nullptr);
    if (!mem_dc_) {
      DeleteObject(dib_);
      dib_ = nullptr;
      return;
    }
    old_dib_ = static_cast<HBITMAP>(SelectObject(mem_dc_, dib_));
    owns_dib_ = true;
    raster_ = new SkiaRaster();
    const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    raster_->surface =
        SkSurfaces::WrapPixels(info, bits, static_cast<size_t>(stride_));
    if (!raster_->surface) {
      destroy_owned();
      return;
    }
    canvas_ = raster_->surface->getCanvas();
    if (!canvas_) {
      destroy_owned();
      return;
    }
    raster_->font = font_for_px(font_px);
    ready_ = true;
  }

  ~SkiaCanvasBackend() override {
    if (owns_dib_) {
      destroy_owned();
    } else {
      canvas_ = nullptr;
      raster_ = nullptr;
      ready_ = false;
    }
  }

  void present_if_owned() override {
    if (owns_dib_ && ready_ && hdc_ && mem_dc_ && width_ > 0 && height_ > 0) {
      BitBlt(hdc_, 0, 0, width_, height_, mem_dc_, 0, 0, SRCCOPY);
    }
  }

  void fill_rect(int x, int y, int w, int h, Color color) override {
    if (!ready_ || !canvas_ || w <= 0 || h <= 0) {
      return;
    }
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(to_sk_color(color));
    canvas_->drawRect(SkRect::MakeXYWH(static_cast<SkScalar>(x),
                                       static_cast<SkScalar>(y),
                                       static_cast<SkScalar>(w),
                                       static_cast<SkScalar>(h)),
                      paint);
  }

  void stroke_rect(int x, int y, int w, int h, Color color,
                   int stroke_width) override {
    if (!ready_ || !canvas_ || w <= 0 || h <= 0) {
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
    canvas_->drawRect(rect, paint);
  }

  void draw_line(int x0, int y0, int x1, int y1, Color color,
                 int stroke_width) override {
    if (!ready_ || !canvas_) {
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
    canvas_->drawLine(static_cast<SkScalar>(x0), static_cast<SkScalar>(y0),
                      static_cast<SkScalar>(x1), static_cast<SkScalar>(y1),
                      paint);
  }

  void draw_text(int x, int y, const wchar_t* text, Color color) override {
    if (!ready_ || !canvas_ || !raster_ || !text || !*text) {
      return;
    }
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(to_sk_color(color));

    SkFontMetrics metrics = {};
    raster_->font.getMetrics(&metrics);
    const SkScalar baseline = static_cast<SkScalar>(y) - metrics.fAscent;
    const size_t len = static_cast<size_t>(lstrlenW(text));
    canvas_->drawSimpleText(text, len * sizeof(wchar_t), SkTextEncoding::kUTF16,
                            static_cast<SkScalar>(x), baseline, raster_->font,
                            paint);
  }

  Size measure_text(const wchar_t* text) const override {
    Size out;
    if (!ready_ || !raster_ || !text || !*text) {
      return out;
    }
    const size_t len = static_cast<size_t>(lstrlenW(text));
    const SkScalar width = raster_->font.measureText(
        text, len * sizeof(wchar_t), SkTextEncoding::kUTF16);
    SkFontMetrics metrics = {};
    raster_->font.getMetrics(&metrics);
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

  void clip_rect(int x, int y, int w, int h) override {
    if (!ready_ || !canvas_ || w <= 0 || h <= 0) {
      return;
    }
    canvas_->clipRect(
        SkRect::MakeXYWH(static_cast<SkScalar>(x), static_cast<SkScalar>(y),
                         static_cast<SkScalar>(w), static_cast<SkScalar>(h)),
        SkClipOp::kIntersect, false);
  }

  void save() override {
    if (!ready_ || !canvas_) {
      return;
    }
    canvas_->save();
  }

  void restore() override {
    if (!ready_ || !canvas_) {
      return;
    }
    if (canvas_->getSaveCount() > 1) {
      canvas_->restore();
    }
  }

  bool ready() const { return ready_; }

 private:
  void destroy_owned() {
    canvas_ = nullptr;
    if (owns_dib_) {
      delete raster_;
    }
    raster_ = nullptr;
    if (mem_dc_) {
      if (old_dib_) {
        SelectObject(mem_dc_, old_dib_);
        old_dib_ = nullptr;
      }
      DeleteDC(mem_dc_);
      mem_dc_ = nullptr;
    }
    if (dib_ && owns_dib_) {
      DeleteObject(dib_);
      dib_ = nullptr;
    }
    pixels_ = nullptr;
    ready_ = false;
    owns_dib_ = false;
  }

  HDC hdc_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  HBITMAP dib_ = nullptr;
  HBITMAP old_dib_ = nullptr;
  HDC mem_dc_ = nullptr;
  void* pixels_ = nullptr;
  int stride_ = 0;
  SkiaRaster* raster_ = nullptr;
  SkCanvas* canvas_ = nullptr;
  bool ready_ = false;
  bool owns_dib_ = false;
};

}  // namespace

CanvasBackend* create_skia_canvas_backend(HDC hdc, int width, int height) {
  auto* backend = new SkiaCanvasBackend(hdc, width, height);
  if (!backend->ready()) {
    delete backend;
    return nullptr;
  }
  return backend;
}

bool skia_canvas_backend_linked() {
  return true;
}

void discard_skia_retained_surface() {
  release_retained();
}

}  // namespace detail
}  // namespace gfx
}  // namespace ui
