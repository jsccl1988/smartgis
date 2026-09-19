// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/gdi/gdi_gdiplus.h"

#include <mutex>

#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace render {
namespace {

std::mutex g_lock;
ULONG_PTR g_token = 0;
bool g_started = false;
bool g_failed = false;

Gdiplus::Color color_from_colorref(COLORREF c, BYTE a = 255) {
  return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

}  // namespace

bool gdiplus_ensure_started() {
  std::lock_guard<std::mutex> lock(g_lock);
  if (g_started) {
    return true;
  }
  if (g_failed) {
    return false;
  }
  Gdiplus::GdiplusStartupInput input;
  const Gdiplus::Status st = Gdiplus::GdiplusStartup(&g_token, &input, nullptr);
  if (st != Gdiplus::Ok) {
    g_failed = true;
    return false;
  }
  g_started = true;
  return true;
}

void gdiplus_shutdown() {
  std::lock_guard<std::mutex> lock(g_lock);
  if (!g_started) {
    return;
  }
  Gdiplus::GdiplusShutdown(g_token);
  g_token = 0;
  g_started = false;
}

bool gdiplus_available() {
  return gdiplus_ensure_started();
}

GdiplusGraphics::GdiplusGraphics(HDC hdc) : hdc_(hdc) {
  if (!hdc || !gdiplus_ensure_started()) {
    return;
  }
  auto* g = new Gdiplus::Graphics(hdc);
  g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  gfx_ = g;
}

GdiplusGraphics::~GdiplusGraphics() {
  delete static_cast<Gdiplus::Graphics*>(gfx_);
  gfx_ = nullptr;
}

bool GdiplusGraphics::draw_polyline(const POINT* pts, int n, COLORREF color,
                                    float width_px) {
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || !pts || n < 2) {
    return false;
  }
  if (width_px < 1.f) {
    width_px = 1.f;
  }
  Gdiplus::Pen pen(color_from_colorref(color), width_px);
  pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound,
                 Gdiplus::DashCapRound);
  pen.SetLineJoin(Gdiplus::LineJoinRound);
  Gdiplus::Point* gp = new Gdiplus::Point[n];
  for (int i = 0; i < n; ++i) {
    gp[i] = Gdiplus::Point(pts[i].x, pts[i].y);
  }
  const Gdiplus::Status st = g->DrawLines(&pen, gp, n);
  delete[] gp;
  return st == Gdiplus::Ok;
}

bool GdiplusGraphics::fill_polygon(const POINT* pts, int n, COLORREF fill,
                                   COLORREF stroke, float stroke_width_px) {
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || !pts || n < 3) {
    return false;
  }
  Gdiplus::Point* gp = new Gdiplus::Point[n];
  for (int i = 0; i < n; ++i) {
    gp[i] = Gdiplus::Point(pts[i].x, pts[i].y);
  }
  Gdiplus::SolidBrush brush(color_from_colorref(fill));
  g->FillPolygon(&brush, gp, n);
  if (stroke_width_px > 0.f) {
    Gdiplus::Pen pen(color_from_colorref(stroke), stroke_width_px);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    g->DrawPolygon(&pen, gp, n);
  }
  delete[] gp;
  return true;
}

bool GdiplusGraphics::draw_string(int x, int y, const wchar_t* text, int px_h,
                                  COLORREF ink, COLORREF halo, int halo_px,
                                  float angle_deg) {
  auto* g = static_cast<Gdiplus::Graphics*>(gfx_);
  if (!g || !text || !text[0]) {
    return false;
  }
  if (px_h < 10) {
    px_h = 10;
  }
  Gdiplus::FontFamily family(L"Microsoft YaHei");
  Gdiplus::Font font(&family, static_cast<Gdiplus::REAL>(px_h),
                     Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::StringFormat fmt;
  fmt.SetAlignment(Gdiplus::StringAlignmentNear);
  fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

  Gdiplus::Matrix saved;
  g->GetTransform(&saved);
  g->TranslateTransform(static_cast<Gdiplus::REAL>(x),
                        static_cast<Gdiplus::REAL>(y));
  g->RotateTransform(angle_deg);

  Gdiplus::SolidBrush ink_brush(color_from_colorref(ink));
  Gdiplus::SolidBrush halo_brush(color_from_colorref(halo));
  const Gdiplus::PointF origin(0.f, 0.f);
  if (halo_px > 0) {
    static const int kOff[8][2] = {{-1, 0}, {1, 0},  {0, -1}, {0, 1},
                                   {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int s = 1; s <= halo_px; ++s) {
      for (const auto& d : kOff) {
        Gdiplus::PointF p(static_cast<Gdiplus::REAL>(d[0] * s),
                          static_cast<Gdiplus::REAL>(d[1] * s));
        g->DrawString(text, -1, &font, p, &fmt, &halo_brush);
      }
    }
  }
  g->DrawString(text, -1, &font, origin, &fmt, &ink_brush);
  g->SetTransform(&saved);
  return true;
}

}  // namespace render
