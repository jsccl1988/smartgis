// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_STYLE_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_STYLE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"
#include "vista/component/map/ir.h"
#include "vista/component/raster/style.h"

namespace content {
namespace detail {

inline LONG gdi_iround(double v) {
  return static_cast<LONG>(v + (v >= 0.0 ? 0.5 : -0.5));
}

// Precomputed world→view scale so mesh vertices skip per-point divides.
struct ViewXform {
  double min_x = 0.0;
  double max_y = 0.0;
  double sx = 1.0;
  double sy = 1.0;
  int width_px = 0;
  int height_px = 0;

  void bind(const vista::View& view) {
    width_px = static_cast<int>(view.width_px);
    height_px = static_cast<int>(view.height_px);
    min_x = view.min_x;
    max_y = view.max_y;
    const double dx = view.max_x - view.min_x;
    const double dy = view.max_y - view.min_y;
    sx = (dx == 0.0) ? 0.0 : static_cast<double>(view.width_px) / dx;
    sy = (dy == 0.0) ? 0.0 : static_cast<double>(view.height_px) / dy;
  }

  POINT map(float x, float y, bool pixel_space) const {
    if (pixel_space) {
      return POINT{gdi_iround(static_cast<double>(x)),
                   gdi_iround(static_cast<double>(y))};
    }
    return POINT{gdi_iround((static_cast<double>(x) - min_x) * sx),
                 gdi_iround((max_y - static_cast<double>(y)) * sy)};
  }
};

inline void item_to_points(const vista::DrawItem& item, const ViewXform& xform,
                           std::vector<POINT>* out) {
  if (!out) {
    return;
  }
  out->clear();
  out->reserve(item.vertices.size());
  const bool pixel = item.pixel_space;
  for (const vista::Vertex& v : item.vertices) {
    out->push_back(xform.map(v.x, v.y, pixel));
  }
}

// Sticky SelectObject: avoid restore/select per DrawItem when brush/pen match.
struct DcStyle {
  HGDIOBJ brush = nullptr;
  HGDIOBJ pen = nullptr;
  bool known = false;

  void ensure(HDC hdc, HBRUSH brush_in, HPEN pen_in) {
    HGDIOBJ b = brush_in ? static_cast<HGDIOBJ>(brush_in)
                         : GetStockObject(NULL_BRUSH);
    HGDIOBJ p =
        pen_in ? static_cast<HGDIOBJ>(pen_in) : GetStockObject(NULL_PEN);
    if (!known || brush != b) {
      SelectObject(hdc, b);
      brush = b;
    }
    if (!known || pen != p) {
      SelectObject(hdc, p);
      pen = p;
    }
    known = true;
  }

  void invalidate() { known = false; }
};

inline bool poly_outside_view(const POINT* p, INT n, int w, int h) {
  if (w <= 0 || h <= 0 || n <= 0 || !p) {
    return false;
  }
  LONG min_x = p[0].x;
  LONG max_x = p[0].x;
  LONG min_y = p[0].y;
  LONG max_y = p[0].y;
  for (INT i = 1; i < n; ++i) {
    min_x = (std::min)(min_x, p[i].x);
    max_x = (std::max)(max_x, p[i].x);
    min_y = (std::min)(min_y, p[i].y);
    max_y = (std::max)(max_y, p[i].y);
  }
  return max_x < 0 || min_x > w || max_y < 0 || min_y > h;
}

inline bool tri_zero_area(const POINT& a, const POINT& b, const POINT& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) == 0;
}

// Force a consistent screen-space orientation so PolyPolygon + WINDING
// does not cancel opposing-winding tessellation tris into cream holes.
inline void orient_tri_positive(POINT* a, POINT* b, POINT* c) {
  if (!a || !b || !c) {
    return;
  }
  const LONG cross =
      (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
  if (cross < 0) {
    const POINT tmp = *b;
    *b = *c;
    *c = tmp;
  }
}

// Opaque BGRA pack. Implementation is vista::raster (software DIB kernels).
using vista::raster::colorref_to_bgra;
using vista::raster::rgba_to_bgra;

// Process-wide GDI handles for map paint — avoids CreateSolidBrush / CreateFont
// churn on every present (cold bootstrap replays export + viewport paint).
// Method bodies live in map2d_frame_gdi.cc (entry TU owns the cache).
struct GdiPaintResourceCache {
  std::mutex mu;
  std::map<COLORREF, HBRUSH> brushes;
  std::map<uint64_t, HPEN> pens;
  std::map<uint64_t, HFONT> fonts;
  HFONT default_text_font = nullptr;

  HBRUSH brush_for(COLORREF c);
  HPEN pen_for(COLORREF c, int width);
  HFONT font_for(int font_px, int esc);
  HFONT default_text();
};

GdiPaintResourceCache& gdi_paint_resources();

// Coalesce same-pen polylines into PolyPolyline (cosmetic tess centerlines).
struct StrokeBatch {
  HPEN pen = nullptr;
  bool active = false;
  bool credit_mesh = false;
  std::vector<POINT> points;
  std::vector<DWORD> counts;
  size_t flush_count = 0;
  size_t mesh_flush_count = 0;

  static constexpr size_t kMaxPoints = 12288;
  static constexpr size_t kMaxPolys = 4096;

  void reset_payload() {
    points.clear();
    counts.clear();
  }

  void flush(HDC hdc, DcStyle* style) {
    if (!active || counts.empty()) {
      reset_payload();
      active = false;
      credit_mesh = false;
      return;
    }
    style->ensure(hdc, nullptr, pen);
    if (counts.size() == 1) {
      Polyline(hdc, points.data(), static_cast<int>(counts[0]));
    } else {
      PolyPolyline(hdc, points.data(), counts.data(),
                   static_cast<DWORD>(counts.size()));
    }
    ++flush_count;
    if (credit_mesh) {
      ++mesh_flush_count;
    }
    reset_payload();
    active = false;
    credit_mesh = false;
  }

  void append(HDC hdc, DcStyle* style, HPEN p, const std::vector<POINT>& pts,
              bool as_mesh = false) {
    if (pts.size() < 2 || !p) {
      return;
    }
    if (active && (pen != p || credit_mesh != as_mesh)) {
      flush(hdc, style);
    }
    if (!active) {
      pen = p;
      active = true;
      credit_mesh = as_mesh;
    }
    if (counts.size() >= kMaxPolys || points.size() + pts.size() > kMaxPoints) {
      flush(hdc, style);
      pen = p;
      active = true;
      credit_mesh = as_mesh;
    }
    points.insert(points.end(), pts.begin(), pts.end());
    counts.push_back(static_cast<DWORD>(pts.size()));
  }
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_STYLE_H_
