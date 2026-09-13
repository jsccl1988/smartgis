// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/geo/geo_ops.h"
#include "algorithm/geo/geometry_traits.h"
#include "algorithm/geo/vector_traits.h"

#include "ogr_geometry.h"

#include <cstdio>

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
  static_assert(geo::vector_traits<render::Vector2>::dimension == 2);
  static_assert(geo::vector_traits<render::Vector3>::dimension == 3);
  static_assert(geo::vector_traits<render::Vector4>::dimension == 4);

  OGRPoint p2(1.0, 2.0);
  expect(geo::geometry_traits<OGRPoint>::coordinate_dimension(p2) == 2,
         "xy point dim");
  p2.setCoordinateDimension(3);
  expect(geo::geometry_traits<OGRGeometry>::coordinate_dimension(p2) == 3,
         "set dim 3");

  OGRLinearRing r1;
  r1.addPoint(0.0, 0.0);
  r1.addPoint(2.0, 0.0);
  r1.addPoint(2.0, 2.0);
  r1.addPoint(0.0, 2.0);
  r1.closeRings();
  OGRPolygon a;
  a.addRing(&r1);

  OGRLinearRing r2;
  r2.addPoint(1.0, 1.0);
  r2.addPoint(3.0, 1.0);
  r2.addPoint(3.0, 3.0);
  r2.addPoint(1.0, 3.0);
  r2.closeRings();
  OGRPolygon b;
  b.addRing(&r2);
  expect(a.Intersects(&b) != FALSE, "squares intersect via OGR");

  OGRPoint origin(0.0, 0.0);
  OGRGeometry* buffered = geo::buffer(origin, 1.0);
  expect(buffered != nullptr && buffered->IsEmpty() == FALSE, "buffer nonempty");
  if (buffered != nullptr) {
    expect(geo::geometry_traits<OGRGeometry>::coordinate_dimension(*buffered) ==
               2,
           "buffer keeps xy dim");
    OGREnvelope env;
    buffered->getEnvelope(&env);
    expect(env.MaxX > 0.5 && env.MinX < -0.5, "buffer extent");
    delete buffered;
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
