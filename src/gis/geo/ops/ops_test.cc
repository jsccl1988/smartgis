// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/ops/buffer.h"
#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/ops/indexed_tin.h"
#include "gis/geo/ops/measure.h"
#include "gis/geo/ops/predicate.h"
#include "gis/geo/ops/shape.h"
#include "gis/geo/ops/vector_traits.h"
#include "gis/envelope.h"

#include "ogr_geometry.h"

#include <cstdio>
#include <string>

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
  static_assert(geo::ogr_geometry_like<OGRPoint>);
  static_assert(geo::ogr_geometry_like<OGRGeometry>);
  static_assert(geo::ogr_geometry_like<OGRPolygon>);
  static_assert(geo::ogr_geometry_like<OGRTriangulatedSurface>);
  static_assert(geo::ogr_geometry_like<OGRTriangle>);

  OGRPoint p2(1.0, 2.0);
  expect(geo::geometry_traits<OGRPoint>::coordinate_dimension(p2) == 2,
         "xy point dim");
  p2.setCoordinateDimension(3);
  expect(geo::geometry_traits<OGRGeometry>::coordinate_dimension(p2) == 3,
         "set dim 3");
  gis::Envelope mbr;
  geo::fill_envelope(p2, &mbr);
  expect(mbr.MinX == 1.0 && mbr.MaxX == 1.0, "fill_envelope point");

  OGRLinearRing r1;
  r1.addPoint(0.0, 0.0);
  r1.addPoint(2.0, 0.0);
  r1.addPoint(2.0, 2.0);
  r1.addPoint(0.0, 2.0);
  r1.closeRings();
  OGRPolygon a;
  a.addRing(&r1);
  a.closeRings();

  OGRLinearRing r2;
  r2.addPoint(1.0, 1.0);
  r2.addPoint(3.0, 1.0);
  r2.addPoint(3.0, 3.0);
  r2.addPoint(1.0, 3.0);
  r2.closeRings();
  OGRPolygon b;
  b.addRing(&r2);
  b.closeRings();
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

  expect(geo::is_valid(a), "square valid");
  expect(geo::is_simple(a), "square simple");
  OGRGeometry* pos = geo::point_on_surface(a);
  expect(pos != nullptr && geo::covers(static_cast<const OGRGeometry&>(a), *pos),
         "point_on_surface in poly");
  delete pos;
  OGRGeometry* mrr = geo::minimum_rotated_rectangle(a);
  expect(mrr != nullptr && !mrr->IsEmpty(), "min rotated rect");
  delete mrr;
  OGRGeometry* uu = geo::unary_union(a);
  expect(uu != nullptr && geo::is_valid(*uu), "unary_union valid");
  delete uu;
  OGRGeometry* dens = geo::densify(a, 0.5);
  expect(dens != nullptr, "densify");
  delete dens;
  double haus = -1.0;
  expect(geo::hausdorff_distance(a, b, &haus) && haus >= 0.0, "hausdorff");
  const std::string de9im = geo::relate_de9im(a, b);
  expect(de9im.size() == 9, "relate DE-9IM length");
  OGRMultiPoint sites;
  sites.addGeometryDirectly(new OGRPoint(0.0, 0.0));
  sites.addGeometryDirectly(new OGRPoint(1.0, 0.0));
  sites.addGeometryDirectly(new OGRPoint(0.0, 1.0));
  sites.addGeometryDirectly(new OGRPoint(1.0, 1.0));
  OGRGeometry* delaunay_mesh = geo::make_delaunay(sites);
  expect(delaunay_mesh != nullptr && !delaunay_mesh->IsEmpty(), "make_delaunay");
  delete delaunay_mesh;
  OGRGeometry* vor = geo::voronoi(sites);
  expect(vor != nullptr && !vor->IsEmpty(), "voronoi");
  delete vor;
  OGRGeometry* repaired = geo::make_valid(a);
  expect(repaired != nullptr && geo::is_valid(*repaired), "make_valid square");
  delete repaired;

  OGRPoint ta(0.0, 0.0, 1.0);
  OGRPoint tb(1.0, 0.0, 1.0);
  OGRPoint tc(0.0, 1.0, 1.0);
  ta.setCoordinateDimension(3);
  tb.setCoordinateDimension(3);
  tc.setCoordinateDimension(3);
  OGRTriangulatedSurface tin;
  expect(geo::add_patch(&tin, ta, tb, tc), "native TIN patch");
  expect(wkbFlatten(tin.getGeometryType()) == wkbTIN, "OGC TIN type");
  expect(tin.getNumGeometries() == 1, "one OGRTriangle");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
