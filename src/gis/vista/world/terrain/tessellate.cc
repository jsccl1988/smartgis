// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/terrain/tessellate.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <vector>

#include "base/memory/arena.h"
#include "base/memory/object_pool.h"
#include "base/trace/event/process_trace.h"
#include "gis/kernel/geo/mesh/geometry.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/provider/impl/ogr/codec/ogr_feature_codec.h"
#include "gis/model/layer/layer.h"

namespace gis {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

struct TessTraceStats {
  std::atomic<int64_t> geom_us{0};
  std::atomic<int64_t> geom_n{0};
  std::atomic<int64_t> poly_fan_us{0};
  std::atomic<int64_t> line_us{0};
  std::atomic<int64_t> line_n{0};
  std::atomic<int64_t> line_points_us{0};
  std::atomic<int64_t> line_dash_us{0};
  std::atomic<int64_t> line_solid_us{0};

  void reset() {
    geom_us.store(0, std::memory_order_relaxed);
    geom_n.store(0, std::memory_order_relaxed);
    poly_fan_us.store(0, std::memory_order_relaxed);
    line_us.store(0, std::memory_order_relaxed);
    line_n.store(0, std::memory_order_relaxed);
    line_points_us.store(0, std::memory_order_relaxed);
    line_dash_us.store(0, std::memory_order_relaxed);
    line_solid_us.store(0, std::memory_order_relaxed);
  }
};

TessTraceStats& tess_trace_stats() {
  static TessTraceStats stats;
  return stats;
}

struct ScopedTessCpu {
  std::atomic<int64_t>* bucket = nullptr;
  std::chrono::steady_clock::time_point begin{};

  explicit ScopedTessCpu(std::atomic<int64_t>* b) {
    if (!base::trace::tracing_enabled() || !b) {
      return;
    }
    bucket = b;
    begin = std::chrono::steady_clock::now();
  }

  ~ScopedTessCpu() {
    if (!bucket) {
      return;
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
    bucket->fetch_add(us, std::memory_order_relaxed);
  }

  ScopedTessCpu(const ScopedTessCpu&) = delete;
  ScopedTessCpu& operator=(const ScopedTessCpu&) = delete;
};

void flush_tess_bucket(const char* name, int64_t us) {
  if (us <= 0) {
    return;
  }
  const auto end = std::chrono::steady_clock::now();
  const auto begin = end - std::chrono::microseconds(us);
  base::trace::process_trace().add(name, "map2d.tess", begin, end);
}

struct Vec2 {
  double x = 0;
  double y = 0;
};

struct PolyPt {
  double x = 0;
  double y = 0;
  double z = 0;
};

// Per-worker scratch pools (tessellate_* runs under parallel_for).
base::ObjectPool<std::vector<PolyPt>>& poly_pt_vec_pool() {
  thread_local base::ObjectPool<std::vector<PolyPt>> pool(
      16, nullptr, [](std::vector<PolyPt>* v) { v->clear(); });
  return pool;
}

base::ObjectPool<std::vector<Vec2>>& vec2_vec_pool() {
  thread_local base::ObjectPool<std::vector<Vec2>> pool(
      8, nullptr, [](std::vector<Vec2>* v) { v->clear(); });
  return pool;
}

base::ObjectPool<std::vector<std::vector<PolyPt>>>& dash_vec_pool() {
  thread_local base::ObjectPool<std::vector<std::vector<PolyPt>>> pool(
      4, nullptr, [](std::vector<std::vector<PolyPt>>* v) { v->clear(); });
  return pool;
}

void clear_tessellate_tls_scratch() {
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(64 * 1024);
  }
}

Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }

double vec_length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

Vec2 vec_normalize(Vec2 v) {
  const double len = vec_length(v);
  if (len < kEps) {
    return {0, 0};
  }
  return {v.x / len, v.y / len};
}

// Left-hand unit normal for a direction (CCW).
Vec2 vec_perp(Vec2 v) { return {-v.y, v.x}; }

double vec_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

double vec_cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

void append_xyz(TessMesh& mesh, float x, float y, float z) {
  mesh.positions.push_back(x);
  mesh.positions.push_back(y);
  mesh.positions.push_back(z);
}

void append_xyz(TessMesh& mesh, Vec2 p, double z) {
  append_xyz(mesh, static_cast<float>(p.x), static_cast<float>(p.y),
             static_cast<float>(z));
}

uint32_t vert_count(const TessMesh& mesh) {
  return static_cast<uint32_t>(mesh.positions.size() / 3);
}

void append_triangle(TessMesh& out, uint32_t a, uint32_t b, uint32_t c) {
  out.indices.push_back(a);
  out.indices.push_back(b);
  out.indices.push_back(c);
}

void append_quad_indices(TessMesh& out, uint32_t base) {
  append_triangle(out, base, base + 1, base + 2);
  append_triangle(out, base + 1, base + 3, base + 2);
}

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

// Legacy per-segment ribbon (fixed half-width 0.05). Kept for
// tessellate_geometry.
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
      out.indices.push_back(base + static_cast<uint32_t>(idx[static_cast<size_t>(i0)]));
      out.indices.push_back(base + static_cast<uint32_t>(idx[static_cast<size_t>(i1)]));
      out.indices.push_back(base + static_cast<uint32_t>(idx[static_cast<size_t>(i2)]));
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

void reset_mesh(TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  out.has_image = false;
}

void append_quad(double min_x, double min_y, double max_x, double max_y,
                 TessMesh& out) {
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(max_y), 0);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(max_y), 0);
  append_quad_indices(out, base);
}

bool rect_has_area(double min_x, double min_y, double max_x, double max_y) {
  return max_x > min_x && max_y > min_y;
}

void emit_segment_quad(const PolyPt& a, const PolyPt& b, Vec2 n, double hw,
                       TessMesh& out) {
  const Vec2 na = n * hw;
  const uint32_t base = vert_count(out);
  append_xyz(out, Vec2{a.x, a.y} + na, a.z);
  append_xyz(out, Vec2{a.x, a.y} - na, a.z);
  append_xyz(out, Vec2{b.x, b.y} + na, b.z);
  append_xyz(out, Vec2{b.x, b.y} - na, b.z);
  append_quad_indices(out, base);
}

// Fan from center covering the exterior wedge between unit vectors u0 and u1.
void emit_round_fan(PolyPt center, Vec2 u0, Vec2 u1, double hw, int segments,
                    TessMesh& out) {
  if (segments < 1) {
    segments = 1;
  }
  const double cross = vec_cross(u0, u1);
  const double dot = vec_dot(u0, u1);
  double angle = std::atan2(cross, dot);
  if (angle < 0) {
    angle += 2.0 * kPi;
  }
  // Prefer the shorter exterior turn (<= pi) for joins; caps pass ~pi.
  if (angle > kPi + 1e-6) {
    angle -= 2.0 * kPi;
  }
  const uint32_t cidx = vert_count(out);
  append_xyz(out, center.x, center.y, static_cast<float>(center.z));
  const int steps = segments;
  for (int i = 0; i <= steps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(steps);
    const double a = t * angle;
    const double ca = std::cos(a);
    const double sa = std::sin(a);
    const Vec2 dir = {u0.x * ca - u0.y * sa, u0.x * sa + u0.y * ca};
    append_xyz(out, Vec2{center.x, center.y} + dir * hw, center.z);
  }
  for (int i = 0; i < steps; ++i) {
    append_triangle(out, cidx, cidx + 1 + static_cast<uint32_t>(i),
                    cidx + 2 + static_cast<uint32_t>(i));
  }
}

void emit_join(const PolyPt& p, Vec2 dir_in, Vec2 dir_out, double hw,
               LineJoin join, double miter_limit, int round_segments,
               TessMesh& out) {
  const Vec2 n0 = vec_perp(dir_in);
  const Vec2 n1 = vec_perp(dir_out);
  const double turn = vec_cross(dir_in, dir_out);
  if (std::fabs(turn) < kEps && vec_dot(dir_in, dir_out) > 0) {
    return;  // nearly colinear, same direction
  }

  // Outer side: opposite the turn direction.
  const bool left_turn = turn > 0;
  const Vec2 outer0 = left_turn ? (n0 * -1.0) : n0;
  const Vec2 outer1 = left_turn ? (n1 * -1.0) : n1;
  const Vec2 a = Vec2{p.x, p.y} + outer0 * hw;
  const Vec2 b = Vec2{p.x, p.y} + outer1 * hw;

  if (join == LineJoin::kRound) {
    emit_round_fan(p, outer0, outer1, hw, round_segments, out);
    return;
  }

  bool use_bevel = (join == LineJoin::kBevel);
  Vec2 miter_pt = a;
  if (!use_bevel) {
    Vec2 miter_dir = vec_normalize(outer0 + outer1);
    if (vec_length(miter_dir) < kEps) {
      use_bevel = true;
    } else {
      const double cos_a = vec_dot(miter_dir, outer0);
      if (std::fabs(cos_a) < kEps) {
        use_bevel = true;
      } else {
        const double miter_len = hw / cos_a;
        if (miter_len > hw * miter_limit || miter_len < 0) {
          use_bevel = true;
        } else {
          miter_pt = Vec2{p.x, p.y} + miter_dir * miter_len;
        }
      }
    }
  }

  const uint32_t base = vert_count(out);
  append_xyz(out, p.x, p.y, static_cast<float>(p.z));
  append_xyz(out, a, p.z);
  if (use_bevel) {
    append_xyz(out, b, p.z);
    append_triangle(out, base, base + 1, base + 2);
  } else {
    append_xyz(out, miter_pt, p.z);
    append_xyz(out, b, p.z);
    append_triangle(out, base, base + 1, base + 2);
    append_triangle(out, base, base + 2, base + 3);
  }
}

void emit_cap_round(const PolyPt& p, Vec2 outward, Vec2 n, double hw,
                    int round_segments, TessMesh& out) {
  // Semicircle from -n to +n (or reverse) that passes through `outward`.
  const Vec2 from = n * -1.0;
  if (vec_cross(from, outward) >= 0) {
    emit_round_fan(p, from, n, hw, round_segments, out);
  } else {
    emit_round_fan(p, n, from, hw, round_segments, out);
  }
}

void emit_end_caps(const std::vector<PolyPt>& pts, double hw, LineCap cap,
                   int round_segments, TessMesh& out) {
  if (pts.size() < 2 || cap == LineCap::kButt) {
    return;
  }
  const Vec2 d0 = vec_normalize({pts[1].x - pts[0].x, pts[1].y - pts[0].y});
  const Vec2 d1 = vec_normalize({pts.back().x - pts[pts.size() - 2].x,
                                 pts.back().y - pts[pts.size() - 2].y});
  if (vec_length(d0) < kEps || vec_length(d1) < kEps) {
    return;
  }
  const Vec2 n0 = vec_perp(d0);
  const Vec2 n1 = vec_perp(d1);

  if (cap == LineCap::kSquare) {
    const PolyPt start_tip{pts[0].x - d0.x * hw, pts[0].y - d0.y * hw,
                           pts[0].z};
    const PolyPt end_tip{pts.back().x + d1.x * hw, pts.back().y + d1.y * hw,
                         pts.back().z};
    emit_segment_quad(start_tip, pts[0], n0, hw, out);
    emit_segment_quad(pts.back(), end_tip, n1, hw, out);
    return;
  }
  emit_cap_round(pts[0], d0 * -1.0, n0, hw, round_segments, out);
  emit_cap_round(pts.back(), d1, n1, hw, round_segments, out);
}

bool append_solid_polyline(const std::vector<PolyPt>& pts, double hw,
                           const LineTessOptions& options, TessMesh& out) {
  ScopedTessCpu cpu(&tess_trace_stats().line_solid_us);
  if (pts.size() < 2 || hw <= 0) {
    return false;
  }

  // Thin / far-zoom strokes: drop round fans (CPU-heavy) and prefer bevel/butt.
  LineJoin join = options.join;
  LineCap cap = options.cap;
  int round_segments = options.round_segments;
  double min_seg = kEps;
  bool skip_joins = false;
  double px = 0;
  if (options.world_units_per_pixel > 0) {
    min_seg = options.world_units_per_pixel * 1.5;
    px = options.pixel_width > 0
             ? options.pixel_width
             : (2.0 * hw / options.world_units_per_pixel);
    if (px <= 2.5) {
      if (join == LineJoin::kRound) {
        join = LineJoin::kBevel;
      }
      if (cap == LineCap::kRound) {
        cap = LineCap::kButt;
      }
      round_segments = 0;
      // Quads abut; join wedges are invisible at ~1–2 px width.
      skip_joins = true;
    } else if (px <= 4.0) {
      if (join == LineJoin::kRound) {
        join = LineJoin::kBevel;
      }
      round_segments = (std::min)(round_segments, 4);
    }
  }

  // Drop short edges (sub-pixel at current wupp) before stroking.
  auto clean_holder = poly_pt_vec_pool().allocate();
  std::vector<PolyPt>& clean = *clean_holder;
  clean.reserve(pts.size());
  clean.push_back(pts.front());
  const double min_seg2 = min_seg * min_seg;
  constexpr size_t kMaxClean = 128;
  for (size_t i = 1; i + 1 < pts.size(); ++i) {
    const double dx = pts[i].x - clean.back().x;
    const double dy = pts[i].y - clean.back().y;
    if (dx * dx + dy * dy >= min_seg2) {
      clean.push_back(pts[i]);
      if (clean.size() >= kMaxClean - 1) {
        break;
      }
    }
  }
  {
    const PolyPt& last = pts.back();
    const double dx = last.x - clean.back().x;
    const double dy = last.y - clean.back().y;
    const double d2 = dx * dx + dy * dy;
    if (d2 >= kEps * kEps) {
      if ((d2 < min_seg2 || clean.size() >= kMaxClean) && clean.size() >= 2) {
        clean.back() = last;
      } else {
        clean.push_back(last);
      }
    }
  }
  if (clean.size() < 2) {
    return false;
  }

  auto dirs_holder = vec2_vec_pool().allocate();
  std::vector<Vec2>& dirs = *dirs_holder;
  dirs.reserve(clean.size() - 1);
  for (size_t i = 0; i + 1 < clean.size(); ++i) {
    dirs.push_back(vec_normalize(
        {clean[i + 1].x - clean[i].x, clean[i + 1].y - clean[i].y}));
  }

  const size_t before = out.indices.size();
  for (size_t i = 0; i + 1 < clean.size(); ++i) {
    emit_segment_quad(clean[i], clean[i + 1], vec_perp(dirs[i]), hw, out);
  }
  if (!skip_joins) {
    for (size_t i = 1; i + 1 < clean.size(); ++i) {
      emit_join(clean[i], dirs[i - 1], dirs[i], hw, join, options.miter_limit,
                round_segments, out);
    }
  }
  emit_end_caps(clean, hw, cap, round_segments, out);
  return out.indices.size() > before;
}

void dash_split(const std::vector<PolyPt>& pts,
                const std::vector<double>& dasharray,
                std::vector<std::vector<PolyPt>>& out_dashes) {
  out_dashes.clear();
  if (pts.size() < 2) {
    return;
  }
  if (dasharray.empty()) {
    out_dashes.push_back(pts);
    return;
  }

  std::vector<double> pattern = dasharray;
  double period = 0;
  for (double& d : pattern) {
    if (d < 0) {
      d = 0;
    }
    period += d;
  }
  if (period <= kEps) {
    out_dashes.push_back(pts);
    return;
  }

  size_t dash_i = 0;
  double remain = pattern[0];
  bool on = true;
  std::vector<PolyPt> cur;
  PolyPt pos = pts[0];
  if (on) {
    cur.push_back(pos);
  }

  auto flush = [&]() {
    if (cur.size() >= 2) {
      out_dashes.push_back(cur);
    }
    cur.clear();
  };

  auto step_pattern = [&]() {
    dash_i = (dash_i + 1) % pattern.size();
    remain = pattern[dash_i];
    const bool next_on = (dash_i % 2) == 0;
    if (on && !next_on) {
      flush();
    } else if (!on && next_on) {
      cur.clear();
      cur.push_back(pos);
    }
    on = next_on;
  };

  for (size_t i = 0; i + 1 < pts.size(); ++i) {
    const PolyPt a = pts[i];
    const PolyPt b = pts[i + 1];
    const Vec2 edge = {b.x - a.x, b.y - a.y};
    const double edge_len = vec_length(edge);
    if (edge_len < kEps) {
      continue;
    }
    const Vec2 unit = edge * (1.0 / edge_len);
    double traveled = 0;
    pos = a;

    while (traveled + kEps < edge_len) {
      while (remain <= kEps) {
        step_pattern();
      }
      const double room = edge_len - traveled;
      const double step = remain < room ? remain : room;
      const double prev = traveled;
      traveled += step;
      remain -= step;
      pos = {a.x + unit.x * traveled, a.y + unit.y * traveled,
             a.z + (b.z - a.z) * (traveled / edge_len)};
      if (on) {
        if (cur.empty()) {
          cur.push_back({a.x + unit.x * prev, a.y + unit.y * prev,
                         a.z + (b.z - a.z) * (prev / edge_len)});
        }
        cur.push_back(pos);
      }
      if (remain <= kEps) {
        step_pattern();
      }
    }
  }
  flush();
}

bool append_styled_polyline(const std::vector<PolyPt>& pts,
                            const LineTessOptions& options, TessMesh& out) {
  const double hw = resolve_line_half_width(options);
  if (pts.size() < 2 || hw <= 0) {
    return false;
  }
  auto dashes_holder = dash_vec_pool().allocate();
  std::vector<std::vector<PolyPt>>& dashes = *dashes_holder;
  {
    ScopedTessCpu cpu(&tess_trace_stats().line_dash_us);
    dash_split(pts, options.dasharray, dashes);
  }
  bool any = false;
  for (const auto& dash : dashes) {
    if (append_solid_polyline(dash, hw, options, out)) {
      any = true;
    }
  }
  return any;
}

std::vector<PolyPt> line_to_points(const OGRLineString* line) {
  std::vector<PolyPt> pts;
  if (!line) {
    return pts;
  }
  const int n = line->getNumPoints();
  pts.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    pts.push_back({line->getX(i), line->getY(i), line->getZ(i)});
  }
  return pts;
}

// Screen-aware OGR → pts. Prefer along-track spacing over index stride so
// meanders (Huang He Ordos "几") are not collapsed into a triangle of extrema.
void line_to_points_decimated(const OGRLineString* line,
                              const LineTessOptions& options,
                              std::vector<PolyPt>& pts) {
  pts.clear();
  if (!line) {
    return;
  }
  const int n = line->getNumPoints();
  if (n < 2) {
    return;
  }

  constexpr int kMaxLineVerts = 1024;
  const double wupp = options.world_units_per_pixel;
  if (wupp > 0) {
    OGREnvelope env;
    line->getEnvelope(&env);
    const double diag =
        std::hypot(env.MaxX - env.MinX, env.MaxY - env.MinY);
    if (diag < 0.5 * wupp) {
      return;
    }
  }

  // Target spacing: ~0.75 px when wupp known, else ~1/400 of path length.
  // Tighter than 1.25 px so Ordos "几" bends survive country-scale framing.
  double path_len = 0;
  for (int i = 0; i + 1 < n; ++i) {
    path_len += std::hypot(line->getX(i + 1) - line->getX(i),
                           line->getY(i + 1) - line->getY(i));
  }
  double spacing = path_len > kEps ? path_len / (kMaxLineVerts - 1) : kEps;
  if (wupp > 0) {
    spacing = (std::max)(spacing, wupp * 0.75);
  }

  int i_n = 0;
  int i_s = 0;
  int i_e = 0;
  int i_w = 0;
  for (int i = 1; i < n; ++i) {
    if (line->getY(i) > line->getY(i_n)) {
      i_n = i;
    }
    if (line->getY(i) < line->getY(i_s)) {
      i_s = i;
    }
    if (line->getX(i) > line->getX(i_e)) {
      i_e = i;
    }
    if (line->getX(i) < line->getX(i_w)) {
      i_w = i;
    }
  }

  std::vector<char> force(static_cast<size_t>(n), 0);
  force[static_cast<size_t>(0)] = 1;
  force[static_cast<size_t>(n - 1)] = 1;
  force[static_cast<size_t>(i_n)] = 1;
  force[static_cast<size_t>(i_s)] = 1;
  force[static_cast<size_t>(i_e)] = 1;
  force[static_cast<size_t>(i_w)] = 1;

  pts.reserve(static_cast<size_t>((std::min)(n, kMaxLineVerts)));
  pts.push_back({line->getX(0), line->getY(0), line->getZ(0)});
  double since = 0;
  for (int i = 1; i < n; ++i) {
    const double x = line->getX(i);
    const double y = line->getY(i);
    const double z = line->getZ(i);
    since += std::hypot(x - line->getX(i - 1), y - line->getY(i - 1));
    const bool take =
        force[static_cast<size_t>(i)] || since >= spacing || i + 1 == n;
    if (!take) {
      continue;
    }
    pts.push_back({x, y, z});
    since = 0;
    if (static_cast<int>(pts.size()) >= kMaxLineVerts && i + 1 < n) {
      // Keep room for forced extrema / endpoint still ahead.
      bool more_force = false;
      for (int j = i + 1; j < n; ++j) {
        if (force[static_cast<size_t>(j)]) {
          more_force = true;
          break;
        }
      }
      if (!more_force) {
        break;
      }
    }
  }
  if (pts.size() >= 2) {
    const double x = line->getX(n - 1);
    const double y = line->getY(n - 1);
    if (std::hypot(pts.back().x - x, pts.back().y - y) > kEps) {
      if (static_cast<int>(pts.size()) >= kMaxLineVerts) {
        pts.back() = {x, y, line->getZ(n - 1)};
      } else {
        pts.push_back({x, y, line->getZ(n - 1)});
      }
    }
  }
}

}  // namespace

double line_half_width_world(double pixel_width, double world_units_per_pixel) {
  if (pixel_width <= 0 || world_units_per_pixel <= 0) {
    return 0;
  }
  return 0.5 * pixel_width * world_units_per_pixel;
}

double line_half_width_from_envelope(double pixel_width,
                                     double envelope_world_width,
                                     double viewport_width_px) {
  if (pixel_width <= 0 || envelope_world_width <= 0 || viewport_width_px <= 0) {
    return 0;
  }
  return line_half_width_world(pixel_width,
                               envelope_world_width / viewport_width_px);
}

double resolve_line_half_width(const LineTessOptions& options) {
  if (options.pixel_width > 0 && options.world_units_per_pixel > 0) {
    return line_half_width_world(options.pixel_width,
                                 options.world_units_per_pixel);
  }
  return options.half_width;
}

bool tessellate_geometry(const OGRGeometry* geom, TessMesh& out) {
  return tessellate_geometry(geom, FillTessOptions{}, out);
}

bool tessellate_geometry(const OGRGeometry* geom,
                         const FillTessOptions& fill_options, TessMesh& out) {
  ScopedTessCpu cpu(&tess_trace_stats().geom_us);
  if (base::trace::tracing_enabled()) {
    tess_trace_stats().geom_n.fetch_add(1, std::memory_order_relaxed);
  }
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!tessellate_geom_into(geom, fill_options, out)) {
    return false;
  }
  return !out.indices.empty();
}

bool tessellate_geoms(const OGRGeometry* const* geoms, size_t count,
                      TessMesh& out) {
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!geoms || count == 0) {
    return false;
  }
  const FillTessOptions opts;
  for (size_t i = 0; i < count; ++i) {
    tessellate_geom_into(geoms[i], opts, out);
  }
  return !out.indices.empty();
}

bool tessellate_layer(OGRLayer* layer, TessMesh& out) {
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  const FillTessOptions opts;
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    tessellate_geom_into(feat->GetGeometryRef(), opts, out);
    OGRFeature::DestroyFeature(feat);
  }
  return !out.indices.empty();
}

bool tessellate_3d_surface(const geo::Tin* surf, TessMesh& out) {
  reset_mesh(out);
  if (!surf) {
    return false;
  }
  const int np = surf->get_point_count();
  const int nt = surf->get_triangle_count();
  for (int i = 0; i < np; ++i) {
    const OGRPoint pt = surf->get_point(i);
    append_xyz(out, static_cast<float>(pt.getX()),
               static_cast<float>(pt.getY()), static_cast<float>(pt.getZ()));
  }
  for (int i = 0; i < nt; ++i) {
    const base::Smt3DTriangle tri = surf->get_triangle(i);
    if (tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= np || tri.b >= np ||
        tri.c >= np) {
      continue;
    }
    out.indices.push_back(static_cast<uint32_t>(tri.a));
    out.indices.push_back(static_cast<uint32_t>(tri.b));
    out.indices.push_back(static_cast<uint32_t>(tri.c));
  }
  return !out.indices.empty();
}

bool tessellate_3d_geometry(const OGRGeometry* geom, TessMesh& out) {
  return tessellate_geometry(geom, out);
}

bool tessellate_arc(const OGRLineString* arc, TessMesh& out) {
  reset_mesh(out);
  if (!arc) {
    return false;
  }
  tessellate_line_legacy(arc, out);
  return !out.indices.empty();
}

bool tessellate_fan(const OGRPolygon* fan, TessMesh& out) {
  reset_mesh(out);
  if (!fan) {
    return false;
  }
  tessellate_polygon(fan, FillTessOptions{}, out);
  return !out.indices.empty();
}

bool tessellate_tin(const geo::Tin* tin, TessMesh& out) {
  reset_mesh(out);
  if (!tin || tin->is_empty()) {
    return false;
  }
  const int np = tin->get_point_count();
  const int nt = tin->get_triangle_count();
  for (int i = 0; i < np; ++i) {
    const OGRPoint pt = tin->get_point(i);
    append_xyz(out, static_cast<float>(pt.getX()),
               static_cast<float>(pt.getY()), 0);
  }
  for (int i = 0; i < nt; ++i) {
    const base::SmtTriangle tri = tin->get_triangle(i);
    if (tri.bDelete || tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= np ||
        tri.b >= np || tri.c >= np) {
      continue;
    }
    out.indices.push_back(static_cast<uint32_t>(tri.a));
    out.indices.push_back(static_cast<uint32_t>(tri.b));
    out.indices.push_back(static_cast<uint32_t>(tri.c));
  }
  return !out.indices.empty();
}

bool tessellate_grid(const geo::Grid* grid, TessMesh& out) {
  reset_mesh(out);
  if (!grid || grid->is_empty()) {
    return false;
  }
  int rows = 0;
  int cols = 0;
  grid->get_size(rows, cols);
  if (rows < 2 || cols < 2) {
    return false;
  }
  for (int i = 0; i + 1 < rows; ++i) {
    for (int j = 0; j + 1 < cols; ++j) {
      const geo::RawPoint p00 = grid->node(i, j);
      const geo::RawPoint p01 = grid->node(i, j + 1);
      const geo::RawPoint p11 = grid->node(i + 1, j + 1);
      const geo::RawPoint p10 = grid->node(i + 1, j);
      const uint32_t base = vert_count(out);
      append_xyz(out, static_cast<float>(p00.x), static_cast<float>(p00.y), 0);
      append_xyz(out, static_cast<float>(p01.x), static_cast<float>(p01.y), 0);
      append_xyz(out, static_cast<float>(p11.x), static_cast<float>(p11.y), 0);
      append_xyz(out, static_cast<float>(p10.x), static_cast<float>(p10.y), 0);
      append_quad_indices(out, base);
    }
  }
  return !out.indices.empty();
}

bool tessellate_raster_layer(const gis::SmtRasterLayer* layer, TessMesh& out) {
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  base::fRect rect;
  if (layer->GetRasterRect(rect) != SMT_ERR_NONE ||
      !rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
    gis::Envelope env;
    layer->get_envelope(env);
    rect.lb.x = static_cast<float>(env.MinX);
    rect.lb.y = static_cast<float>(env.MinY);
    rect.rt.x = static_cast<float>(env.MaxX);
    rect.rt.y = static_cast<float>(env.MaxY);
  }
  if (!rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
    return false;
  }
  append_quad(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y, out);
  char* buf = nullptr;
  long size = 0;
  long code = 0;
  base::fRect loc;
  if (layer->GetRasterNoClone(buf, size, loc, code) == SMT_ERR_NONE && buf &&
      size > 0) {
    out.has_image = true;
  }
  return !out.indices.empty();
}

bool tessellate_tile_layer(const gis::SmtTileLayer* layer, TessMesh& out) {
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  const int n = layer->GetTileCount();
  for (int i = 0; i < n; ++i) {
    const base::SmtTile* tile = layer->GetTile(i);
    if (!tile) {
      continue;
    }
    const base::fRect& rect = tile->rtTileRect;
    if (!rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
      continue;
    }
    append_quad(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y, out);
    if (tile->pTileBuf && tile->lTileBufSize > 0) {
      out.has_image = true;
    }
  }
  if (!out.indices.empty()) {
    return true;
  }
  gis::Envelope env;
  layer->get_envelope(env);
  if (!rect_has_area(env.MinX, env.MinY, env.MaxX, env.MaxY)) {
    return false;
  }
  append_quad(env.MinX, env.MinY, env.MaxX, env.MaxY, out);
  return !out.indices.empty();
}

bool tessellate_line(const OGRLineString* line, TessMesh& out) {
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!line || line->getNumPoints() < 2) {
    return false;
  }
  tessellate_line_legacy(line, out);
  return !out.indices.empty();
}

bool tessellate_line(const OGRLineString* line, const LineTessOptions& options,
                     TessMesh& out) {
  ScopedTessCpu cpu(&tess_trace_stats().line_us);
  if (base::trace::tracing_enabled()) {
    tess_trace_stats().line_n.fetch_add(1, std::memory_order_relaxed);
  }
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!line) {
    return false;
  }
  auto pts_holder = poly_pt_vec_pool().allocate();
  std::vector<PolyPt>& pts = *pts_holder;
  {
    ScopedTessCpu points_cpu(&tess_trace_stats().line_points_us);
    line_to_points_decimated(line, options, pts);
  }
  return append_styled_polyline(pts, options, out);
}

void reset_tess_trace_stats() {
  tess_trace_stats().reset();
}

void flush_tess_trace_stats() {
  if (!base::trace::tracing_enabled()) {
    return;
  }
  TessTraceStats& s = tess_trace_stats();
  // One Complete span per bucket; duration is CPU-us sum (may exceed wall
  // under parallel_for — that is intentional for cost attribution).
  flush_tess_bucket("tess_geom", s.geom_us.load(std::memory_order_relaxed));
  flush_tess_bucket("tess_poly_fan",
                    s.poly_fan_us.load(std::memory_order_relaxed));
  flush_tess_bucket("tess_line", s.line_us.load(std::memory_order_relaxed));
  flush_tess_bucket("tess_line_points",
                    s.line_points_us.load(std::memory_order_relaxed));
  flush_tess_bucket("tess_line_dash",
                    s.line_dash_us.load(std::memory_order_relaxed));
  flush_tess_bucket("tess_line_solid",
                    s.line_solid_us.load(std::memory_order_relaxed));
  base::trace::process_trace().add_counter(
      "tess_geom_n", "map2d.tess",
      s.geom_n.load(std::memory_order_relaxed));
  base::trace::process_trace().add_counter(
      "tess_line_n", "map2d.tess",
      s.line_n.load(std::memory_order_relaxed));
}

bool tessellate_polyline(const float* xyz, size_t point_count,
                         size_t stride_floats, const LineTessOptions& options,
                         TessMesh& out) {
  clear_tessellate_tls_scratch();
  reset_mesh(out);
  if (!xyz || point_count < 2 || (stride_floats != 2 && stride_floats != 3)) {
    return false;
  }
  auto pts_holder = poly_pt_vec_pool().allocate();
  std::vector<PolyPt>& pts = *pts_holder;
  pts.reserve(point_count);
  for (size_t i = 0; i < point_count; ++i) {
    const float* p = xyz + i * stride_floats;
    pts.push_back({p[0], p[1], stride_floats == 3 ? p[2] : 0.f});
  }
  return append_styled_polyline(pts, options, out);
}

bool tessellate_aabb(double min_x, double min_y, double min_z, double max_x,
                     double max_y, double max_z, TessMesh& out) {
  reset_mesh(out);
  if (max_x < min_x || max_y < min_y || max_z < min_z) {
    return false;
  }
  if (max_x - min_x < 1e-9) {
    max_x = min_x + 1e-3;
  }
  if (max_y - min_y < 1e-9) {
    max_y = min_y + 1e-3;
  }
  if (max_z - min_z < 1e-9) {
    max_z = min_z + 1e-3;
  }
  const float x0 = static_cast<float>(min_x);
  const float y0 = static_cast<float>(min_y);
  const float z0 = static_cast<float>(min_z);
  const float x1 = static_cast<float>(max_x);
  const float y1 = static_cast<float>(max_y);
  const float z1 = static_cast<float>(max_z);
  const float corners[8][3] = {
      {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
      {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
  };
  for (int i = 0; i < 8; ++i) {
    out.positions.push_back(corners[i][0]);
    out.positions.push_back(corners[i][1]);
    out.positions.push_back(corners[i][2]);
  }
  const uint32_t faces[12][3] = {
      {0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1},
      {1, 5, 6}, {1, 6, 2}, {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0},
  };
  for (int i = 0; i < 12; ++i) {
    out.indices.push_back(faces[i][0]);
    out.indices.push_back(faces[i][1]);
    out.indices.push_back(faces[i][2]);
  }
  return true;
}

}  // namespace gis
