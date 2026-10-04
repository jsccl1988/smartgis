// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/fill_tess.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "vista/mesh/mesh_append.h"
#include "vista/mesh/mesh_scratch.h"
#include "vista/mesh/tess_trace.h"
#include "ogrsf_frmts.h"

namespace vista {
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
  if (wupp > 0 && diag < 0.5 * wupp) {
    return;
  }

  const int max_verts =
      opts.max_fan_verts > 3 ? opts.max_fan_verts : 8192;

  // Envelope extrema (always retained when decimating).
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

  std::vector<int> keep;
  keep.reserve(static_cast<size_t>((std::min)(n, max_verts) + 8));
  if (n <= max_verts) {
    // Under budget: keep every vertex. RDP on coastal provinces can create
    // self-touching rings that ear-clip "completes" while dropping Yunnan /
    // Guangxi / Taiwan into ocean (city labels float on #aad3df).
    for (int i = 0; i < n; ++i) {
      keep.push_back(i);
    }
  } else {
    // Screen-aware tolerance: ~0.18 px when wupp known (was 0.35 — too
    // aggressive on china overview and punched southern land holes).
    double tol = diag > kEps ? diag / 600.0 : kEps;
    if (wupp > 0) {
      tol = (std::max)(tol, wupp * 0.18);
    }
    const double tol2 = tol * tol;

    std::vector<char> keep_mark(static_cast<size_t>(n), 0);
    keep_mark[0] = 1;
    keep_mark[static_cast<size_t>(n - 1)] = 1;
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
          const double t = ((px - ax) * dx + (py - ay) * dy) / len2;
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

  // Degenerate / unit triangles: one indexed tri (matches world_test /
  // unified_draw_test). Center-fan would emit three wedges (9 indices).
  if (pts.size() == 3) {
    const uint32_t base = vert_count(out);
    for (const PolyPt& p : pts) {
      append_xyz(out, static_cast<float>(p.x), static_cast<float>(p.y), 0);
    }
    out.indices.push_back(base);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
    return;
  }

  // Fan from a true interior seed (OGR PointOnSurface). Ear-clip on
  // RDP-thinned china admin rings stalls or drops remainders and punches
  // rectangular ocean holes under city labels; fan-from-0 chords the
  // Mongolia bite. Interior-seed fan covers any simple concave ring.
  auto cross = [](double ax, double ay, double bx, double by, double cx,
                  double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  };
  auto winding_contains = [&](double px, double py) {
    double w = 0.0;
    const int n = static_cast<int>(pts.size());
    for (int i = 0; i < n; ++i) {
      const PolyPt& a = pts[static_cast<size_t>(i)];
      const PolyPt& b = pts[static_cast<size_t>((i + 1) % n)];
      if (a.y <= py) {
        if (b.y > py && cross(a.x, a.y, b.x, b.y, px, py) > 0.0) {
          w += 1.0;
        }
      } else if (b.y <= py && cross(a.x, a.y, b.x, b.y, px, py) < 0.0) {
        w -= 1.0;
      }
    }
    return w != 0.0;
  };

  const int m0 = static_cast<int>(pts.size());
  double cx = 0.0;
  double cy = 0.0;
  bool have_seed = false;
  // Thread-safe interior seed (emit_fills parallel_for). Do not call OGR
  // PointOnSurface / GEOS here — not safe under concurrent tessellation and
  // ExitProcess(-1) mid browse export.
  auto try_seed = [&](double sx, double sy) {
    if (!have_seed && winding_contains(sx, sy)) {
      cx = sx;
      cy = sy;
      have_seed = true;
    }
  };
  for (int i = 0; i < m0 && !have_seed; ++i) {
    const PolyPt& a = pts[static_cast<size_t>(i)];
    const PolyPt& b = pts[static_cast<size_t>((i + 1) % m0)];
    const PolyPt& c = pts[static_cast<size_t>((i + 2) % m0)];
    try_seed((a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0);
  }
  if (!have_seed) {
    double min_x = pts[0].x;
    double max_x = min_x;
    double min_y = pts[0].y;
    double max_y = min_y;
    for (const PolyPt& p : pts) {
      min_x = (std::min)(min_x, p.x);
      max_x = (std::max)(max_x, p.x);
      min_y = (std::min)(min_y, p.y);
      max_y = (std::max)(max_y, p.y);
    }
    constexpr int kGrid = 24;
    for (int gy = 0; gy < kGrid && !have_seed; ++gy) {
      for (int gx = 0; gx < kGrid && !have_seed; ++gx) {
        const double sx = min_x + (static_cast<double>(gx) + 0.5) *
                                      (max_x - min_x) /
                                      static_cast<double>(kGrid);
        const double sy = min_y + (static_cast<double>(gy) + 0.5) *
                                      (max_y - min_y) /
                                      static_cast<double>(kGrid);
        try_seed(sx, sy);
      }
    }
  }
  if (!have_seed) {
    for (const PolyPt& p : pts) {
      cx += p.x;
      cy += p.y;
    }
    const double inv = 1.0 / static_cast<double>(m0);
    cx *= inv;
    cy *= inv;
  }

  const uint32_t base = vert_count(out);
  for (const PolyPt& p : pts) {
    append_xyz(out, static_cast<float>(p.x), static_cast<float>(p.y), 0);
  }
  const uint32_t cent = vert_count(out);
  append_xyz(out, static_cast<float>(cx), static_cast<float>(cy), 0.f);
  const bool filter_tris = !have_seed;
  for (int i = 0; i < m0; ++i) {
    const int ia = i;
    const int ib = (i + 1) % m0;
    if (filter_tris) {
      const PolyPt& a = pts[static_cast<size_t>(ia)];
      const PolyPt& b = pts[static_cast<size_t>(ib)];
      const double tx = (cx + a.x + b.x) / 3.0;
      const double ty = (cy + a.y + b.y) / 3.0;
      if (!winding_contains(tx, ty)) {
        continue;
      }
    }
    out.indices.push_back(cent);
    out.indices.push_back(base + static_cast<uint32_t>(ia));
    out.indices.push_back(base + static_cast<uint32_t>(ib));
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
}  // namespace vista
