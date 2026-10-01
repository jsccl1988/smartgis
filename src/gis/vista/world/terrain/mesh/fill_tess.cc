// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/terrain/mesh/fill_tess.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gis/vista/world/terrain/mesh/mesh_append.h"
#include "gis/vista/world/terrain/mesh/mesh_scratch.h"
#include "gis/vista/world/terrain/mesh/tess_trace.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {

void tessellate_point_xy(double x, double y, double z, TessMesh& out) {
  const float s = 0.05f;
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(x), static_cast<float>(y) + s,
             static_cast<float>(z));
  append_xyz(out, static_cast<float>(x) - s, static_cast<float>(y) - s,
             static_cast<float>(z));
  append_xyz(out, static_cast<float>(x) + s, static_cast<float>(y) - s,
             static_cast<float>(z));
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
}

void tessellate_segment(double ax, double ay, double az, double bx, double by,
                        double bz, TessMesh& out) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len = std::sqrt(dx * dx + dy * dy);
  double nx = 0;
  double ny = 0.05;
  if (len > 1e-9) {
    nx = (-dy / len) * 0.05;
    ny = (dx / len) * 0.05;
  }
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(ax + nx), static_cast<float>(ay + ny),
             static_cast<float>(az));
  append_xyz(out, static_cast<float>(ax - nx), static_cast<float>(ay - ny),
             static_cast<float>(az));
  append_xyz(out, static_cast<float>(bx + nx), static_cast<float>(by + ny),
             static_cast<float>(bz));
  append_xyz(out, static_cast<float>(bx - nx), static_cast<float>(by - ny),
             static_cast<float>(bz));
  append_quad_indices(out, base);
}

void tessellate_line_legacy(const OGRLineString* line, TessMesh& out) {
  if (!line) {
    return;
  }
  const int n = line->getNumPoints();
  for (int i = 0; i + 1 < n; ++i) {
    tessellate_segment(line->getX(i), line->getY(i), line->getZ(i),
                       line->getX(i + 1), line->getY(i + 1), line->getZ(i + 1),
                       out);
  }
}

void tessellate_ring_fan(const OGRLinearRing* ring, const FillTessOptions& opts,
                         TessMesh& out) {
  if (!ring) {
    return;
  }
  int n = ring->getNumPoints();
  if (n >= 2 && ring->getX(0) == ring->getX(n - 1) &&
      ring->getY(0) == ring->getY(n - 1)) {
    --n;
  }
  if (n < 3) {
    return;
  }

  OGREnvelope env;
  ring->getEnvelope(&env);
  const double diag = std::hypot(env.MaxX - env.MinX, env.MaxY - env.MinY);
  const double wupp = opts.world_units_per_pixel;
  // Sub-pixel fill: skip (wash already covers tiny scraps at carto zooms).
  if (wupp > 0 && diag < 0.75 * wupp) {
    return;
  }

  const int max_verts =
      opts.max_fan_verts > 3 ? opts.max_fan_verts : 1024;
  // Screen-aware tolerance: ~0.75 px when wupp known, else ~1/400 of diag.
  double tol = diag > kEps ? diag / 400.0 : kEps;
  if (wupp > 0) {
    tol = (std::max)(tol, wupp * 0.75);
  }
  const double tol2 = tol * tol;

  // Douglas–Peucker on the open ring so concave frontiers (Mongolia bite)
  // keep bends instead of collapsing into two long chords under stride.
  std::vector<char> keep_mark(static_cast<size_t>(n), 0);
  keep_mark[0] = 1;
  keep_mark[static_cast<size_t>(n - 1)] = 1;
  // Always retain envelope extrema (ring order).
  int i_n = 0;
  int i_s = 0;
  int i_e = 0;
  int i_w = 0;
  for (int i = 1; i < n; ++i) {
    const double x = ring->getX(i);
    const double y = ring->getY(i);
    if (y > ring->getY(i_n)) {
      i_n = i;
    }
    if (y < ring->getY(i_s)) {
      i_s = i;
    }
    if (x > ring->getX(i_e)) {
      i_e = i;
    }
    if (x < ring->getX(i_w)) {
      i_w = i;
    }
  }
  keep_mark[static_cast<size_t>(i_n)] = 1;
  keep_mark[static_cast<size_t>(i_s)] = 1;
  keep_mark[static_cast<size_t>(i_e)] = 1;
  keep_mark[static_cast<size_t>(i_w)] = 1;

  struct Seg {
    int a;
    int b;
  };
  std::vector<Seg> stack;
  stack.push_back({0, n - 1});
  while (!stack.empty()) {
    const Seg s = stack.back();
    stack.pop_back();
    if (s.b <= s.a + 1) {
      continue;
    }
    const double ax = ring->getX(s.a);
    const double ay = ring->getY(s.a);
    const double bx = ring->getX(s.b);
    const double by = ring->getY(s.b);
    const double dx = bx - ax;
    const double dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    int farthest = -1;
    double best = tol2;
    for (int i = s.a + 1; i < s.b; ++i) {
      const double px = ring->getX(i);
      const double py = ring->getY(i);
      double d2;
      if (len2 <= kEps) {
        const double ex = px - ax;
        const double ey = py - ay;
        d2 = ex * ex + ey * ey;
      } else {
        // Perp distance² to segment a→b.
        const double t =
            ((px - ax) * dx + (py - ay) * dy) / len2;
        const double qx = ax + t * dx;
        const double qy = ay + t * dy;
        const double ex = px - qx;
        const double ey = py - qy;
        d2 = ex * ex + ey * ey;
      }
      if (d2 > best) {
        best = d2;
        farthest = i;
      }
    }
    if (farthest < 0) {
      continue;
    }
    keep_mark[static_cast<size_t>(farthest)] = 1;
    stack.push_back({s.a, farthest});
    stack.push_back({farthest, s.b});
  }

  std::vector<int> keep;
  keep.reserve(static_cast<size_t>((std::min)(n, max_verts) + 8));
  for (int i = 0; i < n; ++i) {
    if (keep_mark[static_cast<size_t>(i)]) {
      keep.push_back(i);
    }
  }
  // If RDP still overshoots the budget, thin uniformly while keeping extrema.
  if (static_cast<int>(keep.size()) > max_verts) {
    std::vector<char> force(static_cast<size_t>(n), 0);
    force[0] = 1;
    force[static_cast<size_t>(n - 1)] = 1;
    force[static_cast<size_t>(i_n)] = 1;
    force[static_cast<size_t>(i_s)] = 1;
    force[static_cast<size_t>(i_e)] = 1;
    force[static_cast<size_t>(i_w)] = 1;
    const int step =
        (static_cast<int>(keep.size()) + max_verts - 1) / max_verts;
    std::vector<int> thinned;
    thinned.reserve(static_cast<size_t>(max_verts));
    for (size_t k = 0; k < keep.size(); ++k) {
      const int i = keep[k];
      if (force[static_cast<size_t>(i)] ||
          (static_cast<int>(k) % step) == 0 || k + 1 == keep.size()) {
        thinned.push_back(i);
      }
    }
    keep.swap(thinned);
  }

  auto pts_holder = poly_pt_vec_pool().allocate();
  std::vector<PolyPt>& pts = *pts_holder;
  pts.reserve(keep.size());
  for (int i : keep) {
    pts.push_back({ring->getX(i), ring->getY(i), 0});
  }
  if (pts.size() < 3) {
    return;
  }

  // Ear clipping keeps the fill boundary on the decimated ring. Fan-from-0
  // plus PIP drops exterior triangles on concave admin rings and leaves a
  // V-shaped northern chord across Inner Mongolia.
  auto cross = [](double ax, double ay, double bx, double by, double cx,
                  double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  };
  auto point_in_tri = [&](double px, double py, const PolyPt& a,
                          const PolyPt& b, const PolyPt& c) {
    const double c1 = cross(a.x, a.y, b.x, b.y, px, py);
    const double c2 = cross(b.x, b.y, c.x, c.y, px, py);
    const double c3 = cross(c.x, c.y, a.x, a.y, px, py);
    const bool has_neg = (c1 < 0) || (c2 < 0) || (c3 < 0);
    const bool has_pos = (c1 > 0) || (c2 > 0) || (c3 > 0);
    return !(has_neg && has_pos);
  };

  double area2 = 0;
  const int m0 = static_cast<int>(pts.size());
  for (int i = 0; i < m0; ++i) {
    const PolyPt& a = pts[static_cast<size_t>(i)];
    const PolyPt& b = pts[static_cast<size_t>((i + 1) % m0)];
    area2 += a.x * b.y - b.x * a.y;
  }
  const bool ccw = area2 > 0;

  std::vector<int> idx(static_cast<size_t>(m0));
  for (int i = 0; i < m0; ++i) {
    idx[static_cast<size_t>(i)] = i;
  }

  const uint32_t base = vert_count(out);
  for (const PolyPt& p : pts) {
    append_xyz(out, static_cast<float>(p.x), static_cast<float>(p.y), 0);
  }

  auto is_ear = [&](int i0, int i1, int i2) {
    const PolyPt& a = pts[static_cast<size_t>(idx[static_cast<size_t>(i0)])];
    const PolyPt& b = pts[static_cast<size_t>(idx[static_cast<size_t>(i1)])];
    const PolyPt& c = pts[static_cast<size_t>(idx[static_cast<size_t>(i2)])];
    const double cr = cross(a.x, a.y, b.x, b.y, c.x, c.y);
    if (ccw ? (cr <= kEps) : (cr >= -kEps)) {
      return false;  // reflex / collinear — not a convex ear tip
    }
    const int m = static_cast<int>(idx.size());
    for (int j = 0; j < m; ++j) {
      if (j == i0 || j == i1 || j == i2) {
        continue;
      }
      const PolyPt& p = pts[static_cast<size_t>(idx[static_cast<size_t>(j)])];
      if (point_in_tri(p.x, p.y, a, b, c)) {
        return false;
      }
    }
    return true;
  };

  int guard = m0 * m0 + 8;
  while (static_cast<int>(idx.size()) > 3 && guard-- > 0) {
    const int m = static_cast<int>(idx.size());
    bool clipped = false;
    for (int i = 0; i < m; ++i) {
      const int i0 = (i + m - 1) % m;
      const int i1 = i;
      const int i2 = (i + 1) % m;
      if (!is_ear(i0, i1, i2)) {
        continue;
      }
      out.indices.push_back(base +
                            static_cast<uint32_t>(idx[static_cast<size_t>(i0)]));
      out.indices.push_back(base +
                            static_cast<uint32_t>(idx[static_cast<size_t>(i1)]));
      out.indices.push_back(base +
                            static_cast<uint32_t>(idx[static_cast<size_t>(i2)]));
      idx.erase(idx.begin() + i1);
      clipped = true;
      break;
    }
    if (!clipped) {
      break;
    }
  }
  if (idx.size() == 3) {
    out.indices.push_back(base + static_cast<uint32_t>(idx[0]));
    out.indices.push_back(base + static_cast<uint32_t>(idx[1]));
    out.indices.push_back(base + static_cast<uint32_t>(idx[2]));
  }
}

void tessellate_polygon(const OGRPolygon* poly, const FillTessOptions& opts,
                        TessMesh& out) {
  ScopedTessCpu cpu(&tess_trace_stats().poly_fan_us);
  if (!poly) {
    return;
  }
  tessellate_ring_fan(poly->getExteriorRing(), opts, out);
}

bool tessellate_geom_into(const OGRGeometry* geom, const FillTessOptions& opts,
                          TessMesh& out) {
  if (!geom) {
    return false;
  }
  switch (wkbFlatten(geom->getGeometryType())) {
    case wkbPoint: {
      const auto* p = geom->toPoint();
      tessellate_point_xy(p->getX(), p->getY(), p->getZ(), out);
      return true;
    }
    case wkbLineString:
    case wkbLinearRing:
      tessellate_line_legacy(geom->toLineString(), out);
      return true;
    case wkbPolygon:
    case wkbTriangle:
      tessellate_polygon(geom->toPolygon(), opts, out);
      return true;
    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbGeometryCollection:
    case wkbTIN: {
      const auto* col = geom->toGeometryCollection();
      if (!col) {
        return false;
      }
      const int n = col->getNumGeometries();
      bool any = false;
      for (int i = 0; i < n; ++i) {
        if (tessellate_geom_into(col->getGeometryRef(i), opts, out)) {
          any = true;
        }
      }
      return any;
    }
    default:
      return false;
  }
}

}  // namespace detail
}  // namespace gis
