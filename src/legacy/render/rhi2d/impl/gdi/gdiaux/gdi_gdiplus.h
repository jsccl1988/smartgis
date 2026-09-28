// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_GDIPLUS_H_
#define SMT_LEGACY_RENDER_GDI_GDIPLUS_H_

#include <windows.h>

namespace render {

// Process-wide GDI+ token. Safe to call repeatedly; returns false if startup
// failed (callers must fall back to GDI).
bool gdiplus_ensure_started();
void gdiplus_shutdown();
bool gdiplus_available();

// Short-lived Graphics wrapper bound to an HDC. Null when GDI+ is unavailable.
class GdiplusGraphics {
 public:
  explicit GdiplusGraphics(HDC hdc);
  ~GdiplusGraphics();

  GdiplusGraphics(const GdiplusGraphics&) = delete;
  GdiplusGraphics& operator=(const GdiplusGraphics&) = delete;

  bool ok() const { return gfx_ != nullptr; }
  void* graphics() const { return gfx_; }

  bool draw_polyline(const POINT* pts, int n, COLORREF color, float width_px);
  bool fill_polygon(const POINT* pts, int n, COLORREF fill, COLORREF stroke,
                    float stroke_width_px);
  bool draw_string(int x, int y, const wchar_t* text, int px_h, COLORREF ink,
                   COLORREF halo, int halo_px, float angle_deg);

 private:
  void* gfx_ = nullptr;  // Gdiplus::Graphics*
  HDC hdc_ = nullptr;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_GDIPLUS_H_
