// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "effect/map/pass.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#pragma comment(lib, "gdiplus.lib")

namespace effect {
namespace map {
namespace {

// Token stays for the process. GdiplusShutdown from a static destructor is
// unsafe, and this TU never creates a window.
bool gdiplus_ready() {
  struct State {
    ULONG_PTR token = 0;
    bool ok = false;
  };
  static const State state = [] {
    State s;
    Gdiplus::GdiplusStartupInput input;
    s.ok = Gdiplus::GdiplusStartup(&s.token, &input, nullptr) == Gdiplus::Ok;
    return s;
  }();
  return state.ok;
}

int encode_utf16(uint32_t codepoint, wchar_t out[3]) {
  if (codepoint > 0x10FFFFu ||
      (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
    return 0;
  }
  if (codepoint <= 0xFFFFu) {
    out[0] = static_cast<wchar_t>(codepoint);
    out[1] = 0;
    return 1;
  }
  const uint32_t u = codepoint - 0x10000u;
  out[0] = static_cast<wchar_t>(0xD800u + (u >> 10));
  out[1] = static_cast<wchar_t>(0xDC00u + (u & 0x3FFu));
  out[2] = 0;
  return 2;
}

std::unique_ptr<Gdiplus::Font> make_font(float text_size_px) {
  const wchar_t* faces[] = {L"Segoe UI", L"Arial", L"Tahoma"};
  for (const wchar_t* face : faces) {
    auto font = std::make_unique<Gdiplus::Font>(
        face, text_size_px, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    if (font->GetLastStatus() == Gdiplus::Ok && font->IsAvailable()) {
      return font;
    }
  }
  return nullptr;
}

bool measure_glyph(const Gdiplus::Font& font, const wchar_t* chars, int n,
                   float* advance, Gdiplus::RectF* box) {
  Gdiplus::Bitmap probe(8, 8, PixelFormat32bppARGB);
  if (probe.GetLastStatus() != Gdiplus::Ok) {
    return false;
  }
  Gdiplus::Graphics graphics(&probe);
  if (graphics.GetLastStatus() != Gdiplus::Ok) {
    return false;
  }
  Gdiplus::RectF bounds;
  if (graphics.MeasureString(chars, n, &font, Gdiplus::PointF(0.f, 0.f),
                             &bounds) != Gdiplus::Ok) {
    return false;
  }
  if (advance) {
    *advance = std::max(0.f, bounds.Width);
  }
  if (box) {
    *box = bounds;
  }
  return true;
}

}  // namespace

float WindowsGlyphRasterizer::advance_px(uint32_t codepoint,
                                         float text_size_px) const {
  if (!gdiplus_ready() || text_size_px <= 0.f) {
    return 0.f;
  }
  wchar_t chars[3] = {};
  const int n = encode_utf16(codepoint, chars);
  if (n <= 0) {
    return 0.f;
  }
  std::unique_ptr<Gdiplus::Font> font = make_font(text_size_px);
  if (!font) {
    return 0.f;
  }
  float advance = 0.f;
  if (!measure_glyph(*font, chars, n, &advance, nullptr)) {
    return 0.f;
  }
  return advance;
}

bool WindowsGlyphRasterizer::rasterize(uint32_t codepoint, float text_size_px,
                                       Bitmap* out) {
  if (!out || !gdiplus_ready() || text_size_px <= 0.f) {
    return false;
  }
  out->width = 0;
  out->height = 0;
  out->rgba.clear();
  out->advance_px = 0.f;

  wchar_t chars[3] = {};
  const int n = encode_utf16(codepoint, chars);
  if (n <= 0) {
    return false;
  }
  std::unique_ptr<Gdiplus::Font> font = make_font(text_size_px);
  if (!font) {
    return false;
  }
  float advance = 0.f;
  Gdiplus::RectF box;
  if (!measure_glyph(*font, chars, n, &advance, &box)) {
    return false;
  }

  const float left = std::min(0.f, box.X);
  const float top = std::min(0.f, box.Y);
  const float right = std::max(box.X + box.Width, 1.f);
  const float bottom = std::max(box.Y + box.Height, 1.f);
  int width = static_cast<int>(std::ceil(right - left)) + 1;
  int height = static_cast<int>(std::ceil(bottom - top)) + 1;
  width = std::clamp(width, 1, 256);
  height = std::clamp(height, 1, 256);

  Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
  if (bitmap.GetLastStatus() != Gdiplus::Ok) {
    return false;
  }
  Gdiplus::Graphics graphics(&bitmap);
  if (graphics.GetLastStatus() != Gdiplus::Ok) {
    return false;
  }
  graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
  if (graphics.DrawString(chars, n, font.get(),
                          Gdiplus::PointF(-left, -top), &brush) !=
      Gdiplus::Ok) {
    return false;
  }

  Gdiplus::Rect lock_rect(0, 0, width, height);
  Gdiplus::BitmapData data;
  if (bitmap.LockBits(&lock_rect, Gdiplus::ImageLockModeRead,
                      PixelFormat32bppARGB, &data) != Gdiplus::Ok) {
    return false;
  }
  if (!data.Scan0) {
    bitmap.UnlockBits(&data);
    return false;
  }
  std::vector<uint8_t> rgba(static_cast<size_t>(width) *
                            static_cast<size_t>(height) * 4u);
  const auto* base = static_cast<const uint8_t*>(data.Scan0);
  for (int y = 0; y < height; ++y) {
    const uint8_t* src = base + static_cast<ptrdiff_t>(y) * data.Stride;
    for (int x = 0; x < width; ++x) {
      const uint8_t* px = src + static_cast<ptrdiff_t>(x) * 4;
      uint8_t* dst =
          rgba.data() +
          (static_cast<size_t>(y) * static_cast<size_t>(width) +
           static_cast<size_t>(x)) *
              4u;
      dst[0] = px[2];
      dst[1] = px[1];
      dst[2] = px[0];
      dst[3] = px[3];
    }
  }
  bitmap.UnlockBits(&data);

  out->width = width;
  out->height = height;
  out->rgba = std::move(rgba);
  out->advance_px = advance;
  return true;
}

}  // namespace map
}  // namespace effect
