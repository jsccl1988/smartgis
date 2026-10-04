// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/ops/indexed_tin.h"
#include "gis/geo/ops/geometry_traits.h"

#include <cstdio>
#include <span>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  static_assert(geo::ogr_geometry_like<OGRTriangulatedSurface>);
  static_assert(geo::ogr_geometry_like<OGRTriangle>);

  OGRPoint a(0.0, 0.0, 1.0);
  OGRPoint b(1.0, 0.0, 2.0);
  OGRPoint c(0.0, 1.0, 3.0);
  a.setCoordinateDimension(3);
  b.setCoordinateDimension(3);
  c.setCoordinateDimension(3);
  OGRTriangulatedSurface native;
  expect(geo::add_patch(&native, a, b, c), "ogr add_patch");
  expect(native.getNumGeometries() == 1, "native triangle count");
  expect(wkbFlatten(native.getGeometryType()) == wkbTIN, "native is wkbTIN");
  expect(geo::geometry_traits<OGRTriangulatedSurface>::coordinate_dimension(
             native) >= 2,
         "native dim");

  const geo::Vertex3 verts[] = {
      {0.0, 0.0, 1.0},
      {1.0, 0.0, 2.0},
      {0.0, 1.0, 3.0},
  };
  geo::IndexedTriangle tri;
  tri.a = 0;
  tri.b = 1;
  tri.c = 2;
  OGRTriangulatedSurface indexed;
  expect(geo::fill_indexed_tin(&indexed, verts,
                               std::span<const geo::IndexedTriangle>(&tri, 1)),
         "fill_indexed_tin");
  expect(indexed.getNumGeometries() == 1, "indexed triangle count");

  OGRLinearRing tri_ring;
  tri_ring.addPoint(0.0, 0.0);
  tri_ring.addPoint(1.0, 0.0);
  tri_ring.addPoint(0.0, 1.0);
  tri_ring.addPoint(0.0, 0.0);
  OGRPolygon tri_poly;
  tri_poly.addRing(&tri_ring);
  OGRTriangulatedSurface from_poly;
  expect(geo::fill_tin_from_ogr(&from_poly, tri_poly),
         "fill_tin_from_ogr polygon");
  expect(from_poly.getNumGeometries() == 1, "polygon tess patches");

  OGRTriangulatedSurface from_native;
  expect(geo::fill_tin_from_ogr(&from_native, native),
         "fill_tin_from_ogr TIN");
  expect(from_native.getNumGeometries() == 1, "copy native patches");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
