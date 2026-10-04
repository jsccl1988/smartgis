// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/hexgrid/sample/sample_volume.h"

#include "base/math/vector.h"
#include "gis/geo/tin/delaunay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace plugin {
namespace detail {
namespace {

struct Quad {
  Xyz sw;
  Xyz se;
  Xyz ne;
  Xyz nw;
};

// Projected-meter pit floor (smaller, offset) — not a unit square.
const Quad k_floor_quad = {
    {395120.0, 3451040.0, 86.2},
    {397080.0, 3450980.0, 92.4},
    {397210.0, 3452420.0, 101.8},
    {395050.0, 3452510.0, 88.6},
};

// Original ground (larger irregular parallelogram, ridge at NE).
const Quad k_surface_quad = {
    {394980.0, 3450920.0, 214.0},
    {397240.0, 3450860.0, 198.5},
    {397380.0, 3452610.0, 286.3},
    {394860.0, 3452680.0, 241.7},
};

Xyz bilinear_xy(const Quad& q, double u, double v, double z) {
  const double ou = 1.0 - u;
  const double ov = 1.0 - v;
  Xyz p;
  p.x = ou * ov * q.sw.x + u * ov * q.se.x + u * v * q.ne.x + ou * v * q.nw.x;
  p.y = ou * ov * q.sw.y + u * ov * q.se.y + u * v * q.ne.y + ou * v * q.nw.y;
  p.z = z;
  return p;
}

double mix(double a, double b, double t) {
  return a + (b - a) * t;
}

double gauss2(double u, double v, double cu, double cv, double s) {
  const double du = u - cu;
  const double dv = v - cv;
  return std::exp(-(du * du + dv * dv) / s);
}

// Synthetic DEM: ridge, valley, and a circular pit. Units are meters.
double surface_z(double u, double v) {
  return 220.0 + 48.0 * std::sin(u * 6.283185307179586) * (v - 0.28) +
         62.0 * gauss2(u, v, 0.72, 0.78, 0.045) +
         28.0 * gauss2(u, v, 0.18, 0.62, 0.03) -
         22.0 * gauss2(u, v, 0.40, 0.38, 0.025);
}

double floor_z(double u, double v) {
  return 90.0 + 11.0 * std::sin(u * 9.42477796076938) +
         7.5 * std::cos(v * 12.566370614359172) -
         24.0 * gauss2(u, v, 0.52, 0.46, 0.055);
}

unsigned lcg_next(unsigned* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

double lcg_unit(unsigned* state) {
  return static_cast<double>(lcg_next(state) >> 8) / 16777216.0;
}

struct HeightTin {
  std::vector<base::Vector3> points;
  std::vector<geo::IndexedTriangle> tris;
};

void append_quad_corners(std::vector<base::Vector3>* pts, const Quad& q) {
  pts->push_back(base::Vector3(static_cast<float>(q.sw.x),
                               static_cast<float>(q.sw.y),
                               static_cast<float>(q.sw.z)));
  pts->push_back(base::Vector3(static_cast<float>(q.se.x),
                               static_cast<float>(q.se.y),
                               static_cast<float>(q.se.z)));
  pts->push_back(base::Vector3(static_cast<float>(q.ne.x),
                               static_cast<float>(q.ne.y),
                               static_cast<float>(q.ne.z)));
  pts->push_back(base::Vector3(static_cast<float>(q.nw.x),
                               static_cast<float>(q.nw.y),
                               static_cast<float>(q.nw.z)));
}

HeightTin build_height_tin(const Quad& q,
                           bool is_surface,
                           unsigned rng_seed) {
  HeightTin tin;
  append_quad_corners(&tin.points, q);
  // Override corner Z from the DEM so the hull matches the field.
  auto set_z = [&](int idx, double u, double v) {
    tin.points[static_cast<size_t>(idx)].z = static_cast<float>(
        is_surface ? surface_z(u, v) : floor_z(u, v));
  };
  set_z(0, 0.0, 0.0);
  set_z(1, 1.0, 0.0);
  set_z(2, 1.0, 1.0);
  set_z(3, 0.0, 1.0);

  unsigned rng = rng_seed;
  constexpr int k_nu = 18;
  constexpr int k_nv = 16;
  tin.points.reserve(4 + static_cast<size_t>(k_nu * k_nv) + 80);
  for (int j = 0; j < k_nv; ++j) {
    for (int i = 0; i < k_nu; ++i) {
      const double u =
          (static_cast<double>(i) + 0.5 + (lcg_unit(&rng) - 0.5) * 0.35) /
          static_cast<double>(k_nu);
      const double v =
          (static_cast<double>(j) + 0.5 + (lcg_unit(&rng) - 0.5) * 0.35) /
          static_cast<double>(k_nv);
      const double uu = std::min(0.995, std::max(0.005, u));
      const double vv = std::min(0.995, std::max(0.005, v));
      const double z = is_surface ? surface_z(uu, vv) : floor_z(uu, vv);
      const Xyz p = bilinear_xy(q, uu, vv, z);
      tin.points.push_back(base::Vector3(static_cast<float>(p.x),
                                         static_cast<float>(p.y),
                                         static_cast<float>(p.z)));
    }
  }
  // Dense irregular outline (embayment-style, not a 4-point rectangle).
  for (int k = 1; k < 24; ++k) {
    const double t = static_cast<double>(k) / 24.0;
    const double bump = 0.012 * std::sin(t * 18.84955592153876);
    const Xyz s0 = bilinear_xy(q, t, bump, 0.0);
    const Xyz s1 = bilinear_xy(q, t, 1.0 - bump, 0.0);
    const Xyz s2 = bilinear_xy(q, bump, t, 0.0);
    const Xyz s3 = bilinear_xy(q, 1.0 - bump, t, 0.0);
    const double z0 = is_surface ? surface_z(t, bump) : floor_z(t, bump);
    const double z1 =
        is_surface ? surface_z(t, 1.0 - bump) : floor_z(t, 1.0 - bump);
    const double z2 = is_surface ? surface_z(bump, t) : floor_z(bump, t);
    const double z3 =
        is_surface ? surface_z(1.0 - bump, t) : floor_z(1.0 - bump, t);
    tin.points.push_back(base::Vector3(static_cast<float>(s0.x),
                                       static_cast<float>(s0.y),
                                       static_cast<float>(z0)));
    tin.points.push_back(base::Vector3(static_cast<float>(s1.x),
                                       static_cast<float>(s1.y),
                                       static_cast<float>(z1)));
    tin.points.push_back(base::Vector3(static_cast<float>(s2.x),
                                       static_cast<float>(s2.y),
                                       static_cast<float>(z2)));
    tin.points.push_back(base::Vector3(static_cast<float>(s3.x),
                                       static_cast<float>(s3.y),
                                       static_cast<float>(z3)));
  }

  if (!geo::delaunay_triangles(tin.tris, tin.points.data(),
                               static_cast<int>(tin.points.size())) ||
      tin.tris.size() < 8) {
    tin.tris.clear();
  }
  return tin;
}

bool barycentric(double x,
                 double y,
                 const base::Vector3& a,
                 const base::Vector3& b,
                 const base::Vector3& c,
                 double* u,
                 double* v,
                 double* w) {
  const double det = (static_cast<double>(b.y) - c.y) *
                         (static_cast<double>(a.x) - c.x) +
                     (static_cast<double>(c.x) - b.x) *
                         (static_cast<double>(a.y) - c.y);
  if (std::fabs(det) < 1e-18) {
    return false;
  }
  *u = ((static_cast<double>(b.y) - c.y) * (x - c.x) +
        (static_cast<double>(c.x) - b.x) * (y - c.y)) /
       det;
  *v = ((static_cast<double>(c.y) - a.y) * (x - c.x) +
        (static_cast<double>(a.x) - c.x) * (y - c.y)) /
       det;
  *w = 1.0 - *u - *v;
  return *u >= -1e-5 && *v >= -1e-5 && *w >= -1e-5;
}

double sample_tin_z(const HeightTin& tin, double x, double y, double fallback) {
  double z = fallback;
  double best = 1e300;
  bool hit = false;
  for (const geo::IndexedTriangle& t : tin.tris) {
    if (t.a < 0 || t.b < 0 || t.c < 0) {
      continue;
    }
    const base::Vector3& a = tin.points[static_cast<size_t>(t.a)];
    const base::Vector3& b = tin.points[static_cast<size_t>(t.b)];
    const base::Vector3& c = tin.points[static_cast<size_t>(t.c)];
    double u = 0;
    double v = 0;
    double w = 0;
    if (barycentric(x, y, a, b, c, &u, &v, &w)) {
      return u * static_cast<double>(a.z) + v * static_cast<double>(b.z) +
             w * static_cast<double>(c.z);
    }
    const double cx =
        (static_cast<double>(a.x) + b.x + c.x) * (1.0 / 3.0) - x;
    const double cy =
        (static_cast<double>(a.y) + b.y + c.y) * (1.0 / 3.0) - y;
    const double d = cx * cx + cy * cy;
    if (d < best) {
      best = d;
      z = (static_cast<double>(a.z) + b.z + c.z) * (1.0 / 3.0);
      hit = true;
    }
  }
  if (hit) {
    return z;
  }
  return fallback;
}

}  // namespace

HexCornerSolve solve_hex_from_quarry_sample(int nx, int ny, int nz) {
  HexCornerSolve fail;
  if (nx < 3 || ny < 3 || nz < 3) {
    fail.message = "{\"error\":\"bad_dims\"}";
    return fail;
  }

  const HeightTin surface =
      build_height_tin(k_surface_quad, true, k_demo_rng_seed);
  const HeightTin floor =
      build_height_tin(k_floor_quad, false, k_demo_rng_seed ^ 0xA5A5A5u);
  if (surface.tris.empty() || floor.tris.empty()) {
    fail.message = "{\"error\":\"tin_failed\"}";
    return fail;
  }

  const int n = nx * ny * nz;
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<double> zs(static_cast<size_t>(n));

  for (int k = 0; k < nz; ++k) {
    const double w = static_cast<double>(k) / static_cast<double>(nz - 1);
    for (int j = 0; j < ny; ++j) {
      const double v = static_cast<double>(j) / static_cast<double>(ny - 1);
      for (int i = 0; i < nx; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(nx - 1);
        const Xyz bot = bilinear_xy(k_floor_quad, u, v, 0.0);
        const Xyz top = bilinear_xy(k_surface_quad, u, v, 0.0);
        const double z_bot =
            sample_tin_z(floor, bot.x, bot.y, floor_z(u, v));
        const double z_top =
            sample_tin_z(surface, top.x, top.y, surface_z(u, v));
        const int idx = k * ny * nx + j * nx + i;
        xs[static_cast<size_t>(idx)] = mix(bot.x, top.x, w);
        ys[static_cast<size_t>(idx)] = mix(bot.y, top.y, w);
        zs[static_cast<size_t>(idx)] = mix(z_bot, z_top, w);
      }
    }
  }

  HexCornerSolve solved =
      solve_hex_from_nodes(nx, ny, nz, std::move(xs), std::move(ys),
                           std::move(zs));
  if (solved.ok) {
    solved.message =
        "{\"ok\":true,\"op\":\"orthogrid3d.create_hex_grid\",\"sample\":"
        "\"quarry_tin\",\"nx\":" +
        std::to_string(nx) + ",\"ny\":" + std::to_string(ny) +
        ",\"nz\":" + std::to_string(nz) + ",\"tin_pts\":" +
        std::to_string(surface.points.size() + floor.points.size()) +
        ",\"tin_tris\":" +
        std::to_string(surface.tris.size() + floor.tris.size()) + "}";
  }
  return solved;
}

}  // namespace detail
}  // namespace plugin
