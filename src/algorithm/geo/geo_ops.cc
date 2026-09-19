// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/geo/geo_ops.h"

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include "ogr_geometry.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace geo {
namespace {

void ignore_geos_message(const char* /*fmt*/, ...) {}

OGRGeometry* buffer_point_disk(const OGRPoint& p, double width) {
  if (!(width > 0.0)) {
    return nullptr;
  }
  auto* ring = new OGRLinearRing();
  constexpr int kSegs = 16;
  constexpr double kTwoPi = 6.28318530717958647692;
  for (int i = 0; i <= kSegs; ++i) {
    const double ang = kTwoPi * static_cast<double>(i) / kSegs;
    ring->addPoint(p.getX() + width * std::cos(ang),
                   p.getY() + width * std::sin(ang));
  }
  auto* poly = new OGRPolygon();
  poly->addRingDirectly(ring);
  return poly;
}

OGRGeometry* from_geos_wkb(GEOSContextHandle_t ctx, const GEOSGeometry* geom) {
  if (!ctx || !geom) {
    return nullptr;
  }
  size_t n = 0;
  unsigned char* wkb = GEOSGeomToWKB_buf_r(ctx, geom, &n);
  if (!wkb || n == 0) {
    return nullptr;
  }
  OGRGeometry* out = nullptr;
  if (OGRGeometryFactory::createFromWkb(wkb, nullptr, &out,
                                        static_cast<int>(n)) != OGRERR_NONE) {
    out = nullptr;
  }
  GEOSFree_r(ctx, wkb);
  return out;
}

OGRGeometry* buffer_via_geos(const OGRGeometry& geom, double width) {
  const int wkb_size = const_cast<OGRGeometry&>(geom).WkbSize();
  if (wkb_size <= 0) {
    return nullptr;
  }
  std::vector<unsigned char> wkb(static_cast<size_t>(wkb_size));
  if (const_cast<OGRGeometry&>(geom).exportToWkb(wkbNDR, wkb.data()) !=
      OGRERR_NONE) {
    return nullptr;
  }
  GEOSContextHandle_t ctx = GEOS_init_r();
  if (!ctx) {
    return nullptr;
  }
  GEOSContext_setNoticeHandler_r(ctx, &ignore_geos_message);
  GEOSContext_setErrorHandler_r(ctx, &ignore_geos_message);
  GEOSGeometry* in =
      GEOSGeomFromWKB_buf_r(ctx, wkb.data(), static_cast<size_t>(wkb_size));
  if (!in) {
    GEOS_finish_r(ctx);
    return nullptr;
  }
  GEOSGeometry* buffered = GEOSBuffer_r(ctx, in, width, 8);
  GEOSGeom_destroy_r(ctx, in);
  OGRGeometry* out = from_geos_wkb(ctx, buffered);
  if (buffered) {
    GEOSGeom_destroy_r(ctx, buffered);
  }
  GEOS_finish_r(ctx);
  return out;
}

}  // namespace

OGRGeometry* buffer_via_geos_or_ogr(const OGRGeometry& geom, double width) {
  // This GDAL stack often has geos_c but OGR::Buffer compiled without GEOS.
  if (const auto* pt = dynamic_cast<const OGRPoint*>(&geom)) {
    return buffer_point_disk(*pt, width);
  }
  OGRGeometry* geos = buffer_via_geos(geom, width);
  if (geos != nullptr && geos->IsEmpty() == FALSE) {
    return geos;
  }
  delete geos;
  return nullptr;
}

}  // namespace geo
