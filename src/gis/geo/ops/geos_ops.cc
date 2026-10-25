// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/ops/measure.h"
#include "gis/geo/ops/set.h"
#include "gis/geo/ops/shape.h"

#include <string>
#include <vector>

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include "ogr_geometry.h"

namespace geo {
namespace {

void ignore_geos_message(const char* /*fmt*/, ...) {}

class GeosSession {
 public:
  GeosSession() {
    ctx_ = GEOS_init_r();
    if (ctx_) {
      GEOSContext_setNoticeHandler_r(ctx_, &ignore_geos_message);
      GEOSContext_setErrorHandler_r(ctx_, &ignore_geos_message);
    }
  }

  ~GeosSession() {
    if (ctx_) {
      GEOS_finish_r(ctx_);
    }
  }

  GeosSession(const GeosSession&) = delete;
  GeosSession& operator=(const GeosSession&) = delete;

  GEOSContextHandle_t get() const { return ctx_; }

 private:
  GEOSContextHandle_t ctx_ = nullptr;
};

GEOSGeometry* to_geos(GEOSContextHandle_t ctx, const OGRGeometry& geom) {
  const int wkb_size = const_cast<OGRGeometry&>(geom).WkbSize();
  if (wkb_size <= 0) {
    return nullptr;
  }
  std::vector<unsigned char> wkb(static_cast<size_t>(wkb_size));
  if (const_cast<OGRGeometry&>(geom).exportToWkb(wkbNDR, wkb.data()) != OGRERR_NONE) {
    return nullptr;
  }
  return GEOSGeomFromWKB_buf_r(ctx, wkb.data(), static_cast<size_t>(wkb_size));
}

OGRGeometry* from_geos(GEOSContextHandle_t ctx, const GEOSGeometry* geom) {
  if (!ctx || !geom) {
    return nullptr;
  }
  size_t n = 0;
  unsigned char* wkb = GEOSGeomToWKB_buf_r(ctx, geom, &n);
  if (!wkb || n == 0) {
    return nullptr;
  }
  OGRGeometry* out = nullptr;
  if (OGRGeometryFactory::createFromWkb(wkb, nullptr, &out, static_cast<int>(n)) !=
      OGRERR_NONE) {
    out = nullptr;
  }
  GEOSFree_r(ctx, wkb);
  return out;
}

void destroy_geos(GEOSContextHandle_t ctx, GEOSGeometry* geom) {
  if (ctx && geom) {
    GEOSGeom_destroy_r(ctx, geom);
  }
}

OGRGeometry* geos_unary(const OGRGeometry& geom,
                        GEOSGeometry* (*fn)(GEOSContextHandle_t, const GEOSGeometry*)) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga && fn ? fn(ctx, ga) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* geos_unary_tol(
    const OGRGeometry& geom,
    double tol,
    GEOSGeometry* (*fn)(GEOSContextHandle_t, const GEOSGeometry*, double)) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga && fn ? fn(ctx, ga, tol) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

bool geos_distance(const OGRGeometry& a,
                   const OGRGeometry& b,
                   double* out,
                   int (*fn)(GEOSContextHandle_t,
                             const GEOSGeometry*,
                             const GEOSGeometry*,
                             double*)) {
  if (!out) {
    return false;
  }
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return false;
  }
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* gb = to_geos(ctx, b);
  double dist = 0.0;
  const int ok = (ga && gb && fn) ? fn(ctx, ga, gb, &dist) : 0;
  destroy_geos(ctx, ga);
  destroy_geos(ctx, gb);
  if (ok != 1) {
    return false;
  }
  *out = dist;
  return true;
}

}  // namespace

OGRGeometry* envelope_polygon(const OGRGeometry& geom) {
  OGREnvelope env;
  const_cast<OGRGeometry&>(geom).getEnvelope(&env);
  auto* ring = new OGRLinearRing();
  ring->addPoint(env.MinX, env.MinY);
  ring->addPoint(env.MaxX, env.MinY);
  ring->addPoint(env.MaxX, env.MaxY);
  ring->addPoint(env.MinX, env.MaxY);
  ring->closeRings();
  auto* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  return poly;
}

OGRGeometry* set_via_geos(const OGRGeometry& a, const OGRGeometry& b, SetOp op) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* gb = to_geos(ctx, b);
  GEOSGeometry* result = nullptr;
  if (ga && gb) {
    switch (op) {
      case SetOp::kIntersection:
        result = GEOSIntersection_r(ctx, ga, gb);
        break;
      case SetOp::kUnion:
        result = GEOSUnion_r(ctx, ga, gb);
        break;
      case SetOp::kDifference:
        result = GEOSDifference_r(ctx, ga, gb);
        break;
      case SetOp::kSymDifference:
        result = GEOSSymDifference_r(ctx, ga, gb);
        break;
    }
  }
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  destroy_geos(ctx, gb);
  return out;
}

OGRGeometry* simplify_via_geos(const OGRGeometry& geom, double tolerance) {
  return geos_unary_tol(geom, tolerance, &GEOSSimplify_r);
}

OGRGeometry* convex_hull_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSConvexHull_r);
}

OGRGeometry* boundary_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSBoundary_r);
}

OGRGeometry* centroid_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSGetCentroid_r);
}

OGRGeometry* make_valid_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSMakeValid_r);
}

OGRGeometry* point_on_surface_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSPointOnSurface_r);
}

OGRGeometry* densify_via_geos(const OGRGeometry& geom, double tolerance) {
  return geos_unary_tol(geom, tolerance, &GEOSDensify_r);
}

OGRGeometry* offset_curve_via_geos(const OGRGeometry& geom, double width) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result =
      ga ? GEOSOffsetCurve_r(ctx, ga, width, 8, GEOSBUF_JOIN_ROUND, 5.0) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* concave_hull_via_geos(const OGRGeometry& geom, double ratio) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  const double clamped = ratio < 0.0 ? 0.0 : (ratio > 1.0 ? 1.0 : ratio);
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga ? GEOSConcaveHull_r(ctx, ga, clamped, 0) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* minimum_rotated_rectangle_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSMinimumRotatedRectangle_r);
}

OGRGeometry* minimum_bounding_circle_via_geos(const OGRGeometry& geom) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  double radius = 0.0;
  GEOSGeometry* center = nullptr;
  GEOSGeometry* result =
      ga ? GEOSMinimumBoundingCircle_r(ctx, ga, &radius, &center) : nullptr;
  destroy_geos(ctx, center);
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* unary_union_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSUnaryUnion_r);
}

OGRGeometry* line_merge_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSLineMerge_r);
}

OGRGeometry* node_via_geos(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSNode_r);
}

OGRGeometry* polygonize_via_geos(const OGRGeometry& geom) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* cuts = nullptr;
  GEOSGeometry* dangles = nullptr;
  GEOSGeometry* invalid = nullptr;
  GEOSGeometry* result =
      ga ? GEOSPolygonize_full_r(ctx, ga, &cuts, &dangles, &invalid) : nullptr;
  destroy_geos(ctx, cuts);
  destroy_geos(ctx, dangles);
  destroy_geos(ctx, invalid);
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* remove_repeated_points_via_geos(const OGRGeometry& geom, double tolerance) {
  return geos_unary_tol(geom, tolerance, &GEOSRemoveRepeatedPoints_r);
}

OGRGeometry* snap_via_geos(const OGRGeometry& geom,
                           const OGRGeometry& reference,
                           double tolerance) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* gb = to_geos(ctx, reference);
  GEOSGeometry* result = (ga && gb) ? GEOSSnap_r(ctx, ga, gb, tolerance) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  destroy_geos(ctx, gb);
  return out;
}

OGRGeometry* delaunay_via_geos(const OGRGeometry& geom, double tolerance) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga ? GEOSDelaunayTriangulation_r(ctx, ga, tolerance, 0) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* voronoi_via_geos(const OGRGeometry& geom, double tolerance) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga ? GEOSVoronoiDiagram_r(ctx, ga, nullptr, tolerance, 0) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

OGRGeometry* shortest_line_via_geos(const OGRGeometry& a, const OGRGeometry& b) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* gb = to_geos(ctx, b);
  GEOSCoordSequence* seq =
      (ga && gb) ? GEOSNearestPoints_r(ctx, ga, gb) : nullptr;
  OGRGeometry* out = nullptr;
  unsigned int seq_size = 0;
  if (seq && GEOSCoordSeq_getSize_r(ctx, seq, &seq_size) != 0 &&
      seq_size >= 2) {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    GEOSCoordSeq_getXY_r(ctx, seq, 0, &x0, &y0);
    GEOSCoordSeq_getXY_r(ctx, seq, 1, &x1, &y1);
    auto* line = new OGRLineString();
    line->addPoint(x0, y0);
    line->addPoint(x1, y1);
    out = line;
  }
  if (seq) {
    GEOSCoordSeq_destroy_r(ctx, seq);
  }
  destroy_geos(ctx, ga);
  destroy_geos(ctx, gb);
  return out;
}

bool hausdorff_distance(const OGRGeometry& a, const OGRGeometry& b, double* out) {
  return geos_distance(a, b, out, &GEOSHausdorffDistance_r);
}

bool frechet_distance(const OGRGeometry& a, const OGRGeometry& b, double* out) {
  return geos_distance(a, b, out, &GEOSFrechetDistance_r);
}

std::string relate_de9im(const OGRGeometry& a, const OGRGeometry& b) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return {};
  }
  GEOSGeometry* ga = to_geos(ctx, a);
  GEOSGeometry* gb = to_geos(ctx, b);
  char* matrix = (ga && gb) ? GEOSRelate_r(ctx, ga, gb) : nullptr;
  std::string out;
  if (matrix) {
    out.assign(matrix);
    GEOSFree_r(ctx, matrix);
  }
  destroy_geos(ctx, ga);
  destroy_geos(ctx, gb);
  return out;
}

}  // namespace geo
