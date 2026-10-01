// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/terrain/mesh/line_tess.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gis/vista/world/terrain/mesh/mesh_append.h"
#include "gis/vista/world/terrain/mesh/mesh_scratch.h"
#include "gis/vista/world/terrain/mesh/tess_trace.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {
namespace {

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

}  // namespace

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

// Screen-aware OGR → pts. Prefer along-track spacing over index stride so
// meanders (Huang He Ordos) are not collapsed into a triangle of extrema.
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
  // Tighter than 1.25 px so Ordos bends survive country-scale framing.
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

}  // namespace detail
}  // namespace gis
