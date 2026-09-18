// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/tin/tin.h"

#include <cstdio>
#include <set>
#include <utility>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool vertices_cover_square(const geo::Tin& tin) {
  if (tin.get_point_count() < 4) {
    return false;
  }
  std::set<std::pair<int, int>> seen;
  for (int i = 0; i < tin.get_point_count(); ++i) {
    const OGRPoint p = tin.get_point(i);
    seen.emplace(static_cast<int>(p.getX()), static_cast<int>(p.getY()));
  }
  return seen.count({0, 0}) && seen.count({1, 0}) && seen.count({1, 1}) &&
         seen.count({0, 1});
}

}  // namespace

int main() {
  render::Vector3 square[4] = {
      {0.f, 0.f, 0.f},
      {1.f, 0.f, 0.f},
      {1.f, 1.f, 0.f},
      {0.f, 1.f, 0.f},
  };

  geo::Tin tin_div;
  expect(create_delaunay_tin_div(&tin_div, square, 4) == SMT_ERR_NONE,
         "create_delaunay_tin_div");
  expect(tin_div.get_triangle_count() >= 2, "div has at least two triangles");
  expect(vertices_cover_square(tin_div), "div keeps all square vertices");

  geo::Tin tin_inc;
  expect(create_delaunay_tin_inc(&tin_inc, square, 4) == SMT_ERR_NONE,
         "create_delaunay_tin_inc");
  expect(tin_inc.get_triangle_count() >= 2, "inc has at least two triangles");
  expect(vertices_cover_square(tin_inc), "inc keeps all square vertices");

  base::dbfPoint ring[4] = {
      {0.0, 0.0},
      {2.0, 0.0},
      {2.0, 2.0},
      {0.0, 2.0},
  };
  std::vector<base::SmtTriangle> mesh;
  expect(divide_polygon_into_tri_mesh(mesh, ring, 4) == SMT_ERR_NONE,
         "divide_polygon_into_tri_mesh");
  expect(static_cast<int>(mesh.size()) >= 2, "constrained mesh >= 2 triangles");

  // OGR rings repeat the first vertex. Indices must stay inside the span.
  base::dbfPoint closed[5] = {
      {0.0, 0.0},
      {2.0, 0.0},
      {2.0, 2.0},
      {0.0, 2.0},
      {0.0, 0.0},
  };
  mesh.clear();
  expect(divide_polygon_into_tri_mesh(mesh, closed, 5) == SMT_ERR_NONE,
         "closed-ring divide_polygon_into_tri_mesh");
  for (const base::SmtTriangle& t : mesh) {
    expect(t.a >= 0 && t.a < 5 && t.b >= 0 && t.b < 5 && t.c >= 0 && t.c < 5,
           "closed-ring triangle indices in range");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
