// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/tin/delaunay.h"

#include <cstdio>
#include <set>
#include <utility>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool vertices_cover_square(const OGRTriangulatedSurface& mesh) {
  std::set<std::pair<int, int>> seen;
  OGRTriangulatedSurface& writable =
      const_cast<OGRTriangulatedSurface&>(mesh);
  const int n = writable.getNumGeometries();
  for (int i = 0; i < n; ++i) {
    OGRPolygon* patch = writable.getGeometryRef(i);
    if (patch == nullptr) {
      continue;
    }
    OGRLinearRing* ring = patch->getExteriorRing();
    if (ring == nullptr) {
      continue;
    }
    const int np = ring->getNumPoints();
    for (int k = 0; k < np && k < 3; ++k) {
      OGRPoint p;
      ring->getPoint(k, &p);
      seen.emplace(static_cast<int>(p.getX()), static_cast<int>(p.getY()));
    }
  }
  return seen.count({0, 0}) && seen.count({1, 0}) && seen.count({1, 1}) &&
         seen.count({0, 1});
}

}  // namespace

int main() {
  base::Vector3 square[4] = {
      {0.f, 0.f, 0.f},
      {1.f, 0.f, 0.f},
      {1.f, 1.f, 0.f},
      {0.f, 1.f, 0.f},
  };

  OGRTriangulatedSurface mesh;
  expect(geo::delaunay(&mesh, square, 4), "geo::delaunay");
  expect(mesh.getNumGeometries() >= 2, "at least two triangles");
  expect(vertices_cover_square(mesh), "keeps all square vertices");
  expect(wkbFlatten(mesh.getGeometryType()) == wkbTIN, "wkbTIN");

  std::vector<geo::IndexedTriangle> tris;
  expect(geo::delaunay_triangles(tris, square, 4), "geo::delaunay_triangles");
  expect(static_cast<int>(tris.size()) >= 2, "triangle index list >= 2");

  base::Vector3 ring[4] = {
      {0.f, 0.f, 0.f},
      {2.f, 0.f, 0.f},
      {2.f, 2.f, 0.f},
      {0.f, 2.f, 0.f},
  };
  tris.clear();
  expect(geo::delaunay_constrained(tris, ring, 4), "geo::delaunay_constrained");
  expect(static_cast<int>(tris.size()) >= 2, "constrained mesh >= 2 triangles");

  base::Vector3 closed[5] = {
      {0.f, 0.f, 0.f},
      {2.f, 0.f, 0.f},
      {2.f, 2.f, 0.f},
      {0.f, 2.f, 0.f},
      {0.f, 0.f, 0.f},
  };
  tris.clear();
  expect(geo::delaunay_constrained(tris, closed, 5),
         "closed-ring delaunay_constrained");
  for (const geo::IndexedTriangle& t : tris) {
    expect(t.a >= 0 && t.a < 5 && t.b >= 0 && t.b < 5 && t.c >= 0 && t.c < 5,
           "closed-ring triangle indices in range");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
