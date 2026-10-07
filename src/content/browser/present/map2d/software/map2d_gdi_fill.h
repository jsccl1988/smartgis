// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_FILL_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_FILL_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <vector>

#include "content/browser/present/map2d/software/map2d_gdi_raster.h"
#include "content/browser/present/map2d/software/map2d_gdi_style.h"

namespace content {
namespace detail {

// Line tessellation emits two triangles per segment (A,B,C / B,D,C) or a
// miter fan (A,B,C / A,C,D). Fold those into one convex quad for GDI.
enum class TessQuad : uint8_t { kNone, kSegment, kFan };

TessQuad classify_tess_quad(const uint32_t* idx, size_t nvert, uint32_t* q);

// Solid scanline fill. Overdraw union — no WINDING cancel holes, no tri edges.
void fill_tri_solid(DibSurface* dib, POINT a, POINT b, POINT c, uint32_t bgra);

struct FillBatch {
  HBRUSH brush = nullptr;
  HPEN pen = nullptr;
  COLORREF color = 0;
  bool active = false;
  bool use_poly_polygon = true;
  std::vector<POINT> points;
  std::vector<INT> counts;
  std::vector<POINT> mesh_pts;
  std::vector<INT> mesh_counts;
  size_t flush_count = 0;
  size_t tri_draw_count = 0;
  size_t mesh_flush_count = 0;
  size_t dib_tri_count = 0;
  int view_w = 0;
  int view_h = 0;
  DibSurface* dib = nullptr;
  bool* dib_dirty = nullptr;

  // Land-fill GDI fallback chunks stay modest: a single PolyPolygon / FillPath
  // over tens of thousands of tessellation tris is pathological (~1s china).
  static constexpr size_t kMaxPoints = 768;
  static constexpr size_t kMaxPolys = 96;
  static constexpr size_t kMeshChunkPolys = 96;

  void reset_payload();
  void mark_dib_dirty();
  void flush_mesh_chunk(HDC hdc);
  void emit_mesh_poly(HDC hdc, const POINT* poly, INT n);
  void flush(HDC hdc, DcStyle* style);
  void append_tris(HDC hdc, DcStyle* style, HBRUSH b, HPEN p, COLORREF c,
                   const std::vector<POINT>& pts,
                   const std::vector<uint32_t>& indices, bool coalesce);
};

// Tessellated kLine (indices.size()>=3): filled stroke quads via FillBatch
// (DIB subpixel edge coverage when bound). Not a cosmetic 1px centerline.
void append_line_mesh(HDC hdc, DcStyle* style, FillBatch* fills, HBRUSH brush,
                      HPEN null_pen, COLORREF color,
                      const std::vector<POINT>& pts,
                      const std::vector<uint32_t>& indices);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_GDI_FILL_H_
