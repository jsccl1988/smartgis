// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// GN-DEP: //src/vista/component/raster:raster

#include "content/browser/present/map2d/software/map2d_gdi_fill.h"

#include <cstdint>
#include <vector>

#include "vista/component/raster/fill.h"

namespace content {
namespace detail {

void fill_tri_solid(DibSurface* dib, POINT a, POINT b, POINT c, uint32_t bgra) {
  vista::raster::fill_tri_solid(dib, a, b, c, bgra);
}

TessQuad classify_tess_quad(const uint32_t* idx, size_t nvert, uint32_t* q) {
  if (!idx || !q) {
    return TessQuad::kNone;
  }
  const uint32_t a = idx[0];
  const uint32_t b = idx[1];
  const uint32_t c = idx[2];
  const uint32_t d = idx[3];
  const uint32_t e = idx[4];
  const uint32_t f = idx[5];
  if (a >= nvert || b >= nvert || c >= nvert || d >= nvert || e >= nvert ||
      f >= nvert) {
    return TessQuad::kNone;
  }
  // emit_segment_quad: (base, base+1, base+2) (base+1, base+3, base+2)
  if (d == b && f == c) {
    q[0] = a;
    q[1] = b;
    q[2] = e;
    q[3] = c;
    return TessQuad::kSegment;
  }
  // miter: (base, base+1, base+2) (base, base+2, base+3)
  if (d == a && e == c) {
    q[0] = a;
    q[1] = b;
    q[2] = c;
    q[3] = f;
    return TessQuad::kFan;
  }
  return TessQuad::kNone;
}

void FillBatch::reset_payload() {
  points.clear();
  counts.clear();
}

void FillBatch::mark_dib_dirty() {
  if (dib_dirty) {
    *dib_dirty = true;
  }
}

void FillBatch::flush_mesh_chunk(HDC hdc) {
  if (mesh_counts.empty()) {
    return;
  }
  if (mesh_counts.size() == 1) {
    Polygon(hdc, mesh_pts.data(), mesh_counts[0]);
  } else {
    PolyPolygon(hdc, mesh_pts.data(), mesh_counts.data(),
                static_cast<int>(mesh_counts.size()));
  }
  ++mesh_flush_count;
  mesh_pts.clear();
  mesh_counts.clear();
}

void FillBatch::emit_mesh_poly(HDC hdc, const POINT* poly, INT n) {
  if (n < 3 || !poly) {
    return;
  }
  if (n == 3 && tri_zero_area(poly[0], poly[1], poly[2])) {
    return;
  }
  if (poly_outside_view(poly, n, view_w, view_h)) {
    return;
  }
  mesh_pts.insert(mesh_pts.end(), poly, poly + n);
  mesh_counts.push_back(n);
  ++tri_draw_count;
  if (mesh_counts.size() >= kMeshChunkPolys) {
    flush_mesh_chunk(hdc);
  }
}

void FillBatch::flush(HDC hdc, DcStyle* style) {
  if (active && !use_poly_polygon) {
    style->ensure(hdc, brush, pen);
    flush_mesh_chunk(hdc);
    active = false;
    reset_payload();
    return;
  }
  if (!active || counts.empty()) {
    reset_payload();
    active = false;
    return;
  }
  style->ensure(hdc, brush, pen);
  // GDI fallback for land tris when the HDC is not a 32bpp DIB section.
  // Mega batches are pathological; BeginPath+FillPath drew wireframe — avoid.
  if (counts.size() == 1) {
    Polygon(hdc, points.data(), counts[0]);
  } else {
    PolyPolygon(hdc, points.data(), counts.data(),
                static_cast<int>(counts.size()));
  }
  ++flush_count;
  reset_payload();
  active = false;
}

void FillBatch::append_tris(HDC hdc, DcStyle* style, HBRUSH b, HPEN p,
                            COLORREF c, const std::vector<POINT>& pts,
                            const std::vector<uint32_t>& indices,
                            bool coalesce) {
  if (pts.empty() || indices.size() < 3) {
    return;
  }
  if (active && (brush != b || pen != p || use_poly_polygon != coalesce ||
                 (use_poly_polygon && color != c && !(dib && dib->valid())))) {
    flush(hdc, style);
  }
  if (!active) {
    brush = b;
    pen = p;
    color = c;
    use_poly_polygon = coalesce;
    active = true;
  }

  // Tessellated line meshes: sticky style + small PolyPolygon chunks.
  // Fold segment-quad index pairs into 4-gons (half the GDI polys).
  if (!coalesce) {
    style->ensure(hdc, brush, pen);
    if (mesh_pts.capacity() < kMeshChunkPolys * 4) {
      mesh_pts.reserve(kMeshChunkPolys * 4);
      mesh_counts.reserve(kMeshChunkPolys);
    }
    const size_t nvert = pts.size();
    size_t i = 0;
    while (i + 2 < indices.size()) {
      if (i + 5 < indices.size()) {
        uint32_t q[4] = {};
        const TessQuad kind = classify_tess_quad(indices.data() + i, nvert, q);
        if (kind != TessQuad::kNone) {
          const POINT quad[4] = {pts[q[0]], pts[q[1]], pts[q[2]], pts[q[3]]};
          emit_mesh_poly(hdc, quad, 4);
          i += 6;
          continue;
        }
      }
      const uint32_t a = indices[i];
      const uint32_t bi = indices[i + 1];
      const uint32_t cidx = indices[i + 2];
      i += 3;
      if (a >= nvert || bi >= nvert || cidx >= nvert) {
        continue;
      }
      const POINT tri[3] = {pts[a], pts[bi], pts[cidx]};
      emit_mesh_poly(hdc, tri, 3);
    }
    return;
  }

  // Land / circle fills: direct DIB scanline when available (fast solid
  // overdraw — cream continents without PolyPolygon cancel holes).
  if (dib && dib->valid()) {
    const uint32_t bgra = colorref_to_bgra(c);
    const size_t nvert = pts.size();
    auto emit_tri = [&](POINT t0, POINT t1, POINT t2) {
      POINT tri[3] = {t0, t1, t2};
      if (tri_zero_area(tri[0], tri[1], tri[2]) ||
          poly_outside_view(tri, 3, view_w, view_h)) {
        return;
      }
      // Qualify: DibSurface ADL also finds vista::raster::fill_tri_solid.
      content::detail::fill_tri_solid(dib, tri[0], tri[1], tri[2], bgra);
      ++dib_tri_count;
    };
    size_t i = 0;
    while (i + 2 < indices.size()) {
      if (i + 5 < indices.size()) {
        uint32_t q[4] = {};
        const TessQuad kind = classify_tess_quad(indices.data() + i, nvert, q);
        if (kind == TessQuad::kFan) {
          emit_tri(pts[q[0]], pts[q[1]], pts[q[2]]);
          emit_tri(pts[q[0]], pts[q[2]], pts[q[3]]);
          i += 6;
          continue;
        }
        if (kind == TessQuad::kSegment) {
          emit_tri(pts[q[0]], pts[q[1]], pts[q[3]]);
          emit_tri(pts[q[1]], pts[q[2]], pts[q[3]]);
          i += 6;
          continue;
        }
      }
      const uint32_t ia = indices[i];
      const uint32_t ib = indices[i + 1];
      const uint32_t ic = indices[i + 2];
      i += 3;
      if (ia >= nvert || ib >= nvert || ic >= nvert) {
        continue;
      }
      emit_tri(pts[ia], pts[ib], pts[ic]);
    }
    mark_dib_dirty();
    active = true;
    return;
  }

  // GDI fallback: fold fan/segment quads; keep chunks modest.
  const size_t nvert = pts.size();
  size_t i = 0;
  while (i + 2 < indices.size()) {
    auto push_poly = [&](const POINT* poly, INT n) {
      if (n < 3) {
        return;
      }
      if (counts.size() >= kMaxPolys ||
          points.size() + static_cast<size_t>(n) > kMaxPoints) {
        flush(hdc, style);
        brush = b;
        pen = p;
        color = c;
        use_poly_polygon = true;
        active = true;
      }
      if (n == 3) {
        POINT tri[3] = {poly[0], poly[1], poly[2]};
        if (tri_zero_area(tri[0], tri[1], tri[2]) ||
            poly_outside_view(tri, 3, view_w, view_h)) {
          return;
        }
        orient_tri_positive(&tri[0], &tri[1], &tri[2]);
        points.push_back(tri[0]);
        points.push_back(tri[1]);
        points.push_back(tri[2]);
        counts.push_back(3);
        return;
      }
      if (n == 4) {
        if (poly_outside_view(poly, n, view_w, view_h)) {
          return;
        }
        // Reverse the whole ring when screen winding is negative. Swapping
        // only the first triangle's v1/v2 turns a convex quad into a bowtie
        // and WINDING fills one lobe (checkerboard cell holes).
        POINT q0 = poly[0];
        POINT q1 = poly[1];
        POINT q2 = poly[2];
        POINT q3 = poly[3];
        const LONG cross =
            (q1.x - q0.x) * (q2.y - q0.y) - (q1.y - q0.y) * (q2.x - q0.x);
        if (cross < 0) {
          const POINT tmp = q1;
          q1 = q3;
          q3 = tmp;
        }
        points.push_back(q0);
        points.push_back(q1);
        points.push_back(q2);
        points.push_back(q3);
        counts.push_back(4);
      }
    };

    if (i + 5 < indices.size()) {
      uint32_t q[4] = {};
      const TessQuad kind = classify_tess_quad(indices.data() + i, nvert, q);
      if (kind != TessQuad::kNone) {
        const POINT quad[4] = {pts[q[0]], pts[q[1]], pts[q[2]], pts[q[3]]};
        push_poly(quad, 4);
        i += 6;
        continue;
      }
    }
    const uint32_t ia = indices[i];
    const uint32_t ib = indices[i + 1];
    const uint32_t ic = indices[i + 2];
    i += 3;
    if (ia >= nvert || ib >= nvert || ic >= nvert) {
      continue;
    }
    const POINT tri[3] = {pts[ia], pts[ib], pts[ic]};
    push_poly(tri, 3);
  }
}

void append_line_mesh(HDC hdc, DcStyle* style, FillBatch* fills, HBRUSH brush,
                      HPEN null_pen, COLORREF color,
                      const std::vector<POINT>& pts,
                      const std::vector<uint32_t>& indices) {
  if (!style || !fills) {
    return;
  }
  // Keep Layout's tessellated stroke width. Collapsing to cosmetic 1px
  // PolyPolyline centerlines made diagonal rivers/borders staircase badly.
  // coalesce=true hits DIB fill_tri_solid (subpixel edge coverage).
  fills->append_tris(hdc, style, brush, null_pen, color, pts, indices,
                     /*coalesce=*/true);
}

}  // namespace detail
}  // namespace content
