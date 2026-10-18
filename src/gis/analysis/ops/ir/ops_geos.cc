// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/ir/ops_geos.h"

#include <vector>

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include "ogr_geometry.h"

namespace gis {
namespace detail {
namespace {

void ignore_geos_message(const char* /*fmt*/, ...) {}

// One reentrant GEOS context for a single kernel call.
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
                        GEOSGeometry* (*fn)(GEOSContextHandle_t, const GEOSGeometry*),
                        double simplify_tol) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = nullptr;
  if (ga) {
    if (fn) {
      result = fn(ctx, ga);
    } else {
      result = GEOSSimplify_r(ctx, ga, simplify_tol);
    }
  }
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
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

OGRGeometry* geos_binary(const OGRGeometry& a, const OGRGeometry& b, GeosBinary op) {
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
      case GeosBinary::kIntersection:
        result = GEOSIntersection_r(ctx, ga, gb);
        break;
      case GeosBinary::kUnion:
        result = GEOSUnion_r(ctx, ga, gb);
        break;
      case GeosBinary::kDifference:
        result = GEOSDifference_r(ctx, ga, gb);
        break;
      case GeosBinary::kSymDifference:
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

OGRGeometry* geos_simplify(const OGRGeometry& geom, double tolerance) {
  return geos_unary(geom, nullptr, tolerance);
}

OGRGeometry* geos_convex_hull(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSConvexHull_r, 0.0);
}

OGRGeometry* geos_boundary(const OGRGeometry& geom) {
  return geos_unary(geom, &GEOSBoundary_r, 0.0);
}

OGRGeometry* geos_centroid(const OGRGeometry& geom) {
  GeosSession session;
  GEOSContextHandle_t ctx = session.get();
  if (!ctx) {
    return nullptr;
  }
  GEOSGeometry* ga = to_geos(ctx, geom);
  GEOSGeometry* result = ga ? GEOSGetCentroid_r(ctx, ga) : nullptr;
  OGRGeometry* out = from_geos(ctx, result);
  destroy_geos(ctx, result);
  destroy_geos(ctx, ga);
  return out;
}

}  // namespace detail
}  // namespace gis
