// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "vista/pass/world/hud/hud_raster.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace vista {
namespace {

void put_px(uint8_t* bgra, uint32_t w, uint32_t h, int x, int y, uint8_t r,
            uint8_t g, uint8_t b, uint8_t a) {
  if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h) ||
      a == 0) {
    return;
  }
  uint8_t* p = bgra + (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4u;
  // Src-over into transparent buffer (dest starts clear).
  const float sa = a / 255.f;
  const float da = p[3] / 255.f;
  const float out_a = sa + da * (1.f - sa);
  if (out_a <= 1e-6f) {
    return;
  }
  p[0] = static_cast<uint8_t>(
      (b * sa + p[0] * da * (1.f - sa)) / out_a + 0.5f);
  p[1] = static_cast<uint8_t>(
      (g * sa + p[1] * da * (1.f - sa)) / out_a + 0.5f);
  p[2] = static_cast<uint8_t>(
      (r * sa + p[2] * da * (1.f - sa)) / out_a + 0.5f);
  p[3] = static_cast<uint8_t>(out_a * 255.f + 0.5f);
}

void stroke_line(uint8_t* bgra, uint32_t w, uint32_t h, float x0, float y0,
                 float x1, float y1, uint8_t r, uint8_t g, uint8_t b) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float steps = (std::max)(std::fabs(dx), std::fabs(dy));
  if (steps < 1.f) {
    put_px(bgra, w, h, static_cast<int>(std::lround(x0)),
           static_cast<int>(std::lround(y0)), r, g, b, 255);
    return;
  }
  const int n = static_cast<int>(steps) + 1;
  for (int i = 0; i <= n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n);
    put_px(bgra, w, h, static_cast<int>(std::lround(x0 + dx * t)),
           static_cast<int>(std::lround(y0 + dy * t)), r, g, b, 255);
  }
}

void stroke_ellipse(uint8_t* bgra, uint32_t w, uint32_t h, float cx, float cy,
                    float radius, uint8_t r, uint8_t g, uint8_t b) {
  constexpr int kSeg = 64;
  float px = cx + radius;
  float py = cy;
  for (int i = 1; i <= kSeg; ++i) {
    const float ang = 6.2831853f * static_cast<float>(i) / kSeg;
    const float nx = cx + radius * std::cos(ang);
    const float ny = cy + radius * std::sin(ang);
    stroke_line(bgra, w, h, px, py, nx, ny, r, g, b);
    px = nx;
    py = ny;
  }
}

void blit_halo_text(HDC hdc, int x, int y, const wchar_t* text, int len,
                    COLORREF fill, COLORREF outline) {
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, outline);
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      TextOutW(hdc, x + dx, y + dy, text, len);
    }
  }
  SetTextColor(hdc, fill);
  TextOutW(hdc, x, y, text, len);
}

bool draw_labels_gdi(const HudFrame& frame, uint8_t* bgra) {
  const uint32_t w = frame.width_px;
  const uint32_t h = frame.height_px;
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = static_cast<LONG>(w);
  bi.bmiHeader.biHeight = -static_cast<LONG>(h);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP dib =
      CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old_bmp = SelectObject(mem, dib);
  // Clear to transparent (alpha 0). GDI TextOut writes opaque RGB; we copy
  // non-black pixels with alpha.
  std::memset(bits, 0, static_cast<size_t>(w) * h * 4u);

  HFONT font = CreateFontW(
      -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
  HFONT status_font = CreateFontW(
      -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = SelectObject(mem, status_font ? status_font : font);

  for (const HudLabel& lab : frame.labels) {
    if (lab.text.empty()) {
      continue;
    }
    wchar_t wbuf[256] = {};
    MultiByteToWideChar(CP_UTF8, 0, lab.text.c_str(), -1, wbuf, 255);
    const int len = static_cast<int>(wcslen(wbuf));
    if (len <= 0) {
      continue;
    }
    SelectObject(mem, lab.halo ? (font ? font : status_font)
                                : (status_font ? status_font : font));
    const COLORREF fill = RGB(lab.rgba[0], lab.rgba[1], lab.rgba[2]);
    if (lab.halo) {
      blit_halo_text(mem, static_cast<int>(lab.x), static_cast<int>(lab.y),
                     wbuf, len, fill, RGB(0, 0, 0));
    } else {
      SetBkMode(mem, TRANSPARENT);
      SetTextColor(mem, fill);
      TextOutW(mem, static_cast<int>(lab.x), static_cast<int>(lab.y), wbuf,
               len);
    }
  }

  // Engine badge bottom-right.
  if (frame.badge.enabled && !frame.badge.text.empty()) {
    wchar_t wname[96] = {};
    MultiByteToWideChar(CP_UTF8, 0, frame.badge.text.c_str(), -1, wname, 95);
    SIZE sz = {};
    SelectObject(mem, status_font ? status_font : font);
    GetTextExtentPoint32W(mem, wname, lstrlenW(wname), &sz);
    const int pad_x = 10;
    const int pad_y = 6;
    const int margin = 14;
    const int box_w = sz.cx + pad_x * 2;
    const int box_h = sz.cy + pad_y * 2;
    const int x = static_cast<int>(w) - margin - box_w;
    const int y = static_cast<int>(h) - margin - box_h;
    if (x >= 0 && y >= 0) {
      HBRUSH brush = CreateSolidBrush(RGB(18, 26, 34));
      RECT rc = {x, y, x + box_w, y + box_h};
      FillRect(mem, &rc, brush);
      DeleteObject(brush);
      HPEN pen = CreatePen(PS_SOLID, 1, RGB(90, 110, 130));
      HGDIOBJ old_pen = SelectObject(mem, pen);
      HGDIOBJ old_br = SelectObject(mem, GetStockObject(NULL_BRUSH));
      Rectangle(mem, x, y, x + box_w, y + box_h);
      SelectObject(mem, old_br);
      SelectObject(mem, old_pen);
      DeleteObject(pen);
      SetBkMode(mem, TRANSPARENT);
      SetTextColor(mem, RGB(245, 248, 252));
      TextOutW(mem, x + pad_x, y + pad_y, wname, lstrlenW(wname));
    }
  }

  // Promote opaque GDI pixels into the HUD BGRA (alpha 255 where drawn).
  auto* src = static_cast<const uint32_t*>(bits);
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const uint32_t c = src[y * w + x];
      const uint8_t b = static_cast<uint8_t>(c & 0xFFu);
      const uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFFu);
      const uint8_t r = static_cast<uint8_t>((c >> 16) & 0xFFu);
      if (r | g | b) {
        put_px(bgra, w, h, static_cast<int>(x), static_cast<int>(y), r, g, b,
               255);
      }
    }
  }

  SelectObject(mem, old_font);
  SelectObject(mem, old_bmp);
  if (font) {
    DeleteObject(font);
  }
  if (status_font) {
    DeleteObject(status_font);
  }
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return true;
}

}  // namespace

bool raster_hud_frame_bgra(const HudFrame& frame,
                           std::vector<uint8_t>* out_bgra) {
  if (!out_bgra || frame.width_px == 0 || frame.height_px == 0 ||
      frame.empty()) {
    return false;
  }
  const size_t nbytes =
      static_cast<size_t>(frame.width_px) * frame.height_px * 4u;
  out_bgra->assign(nbytes, 0);
  uint8_t* bgra = out_bgra->data();

  for (const HudLine& ln : frame.lines) {
    stroke_line(bgra, frame.width_px, frame.height_px, ln.x0, ln.y0, ln.x1,
                ln.y1, ln.rgba[0], ln.rgba[1], ln.rgba[2]);
  }
  if (frame.compass.enabled) {
    stroke_ellipse(bgra, frame.width_px, frame.height_px, frame.compass.cx,
                   frame.compass.cy, frame.compass.radius, 210, 225, 240);
    const float nx = std::sin(frame.compass.north_angle_rad);
    const float ny = -std::cos(frame.compass.north_angle_rad);
    const float tip_x =
        frame.compass.cx + nx * (frame.compass.radius - 6.f);
    const float tip_y =
        frame.compass.cy + ny * (frame.compass.radius - 6.f);
    stroke_line(bgra, frame.width_px, frame.height_px, frame.compass.cx,
                frame.compass.cy, tip_x, tip_y, 220, 60, 50);
  }
  (void)draw_labels_gdi(frame, bgra);
  return true;
}

}  // namespace vista
