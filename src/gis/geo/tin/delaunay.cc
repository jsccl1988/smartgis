// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/tin/delaunay.h"

#define GEOS_USE_ONLY_R_API
#include "geos_c.h"

#include "base/math/scalar/constants.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace geo {
namespace detail {

bool xy_eq(float a, float b) {
  return std::fabs(a - b) <= base::kEpsilon;
}

struct GeosContext {
  GEOSContextHandle_t handle = nullptr;

  GeosContext() : handle(GEOS_init_r()) {
    if (handle != nullptr) {
      GEOSContext_setNoticeHandler_r(handle, &ignore_message);
      GEOSContext_setErrorHandler_r(handle, &ignore_message);
    }
  }

  ~GeosContext() {
    if (handle != nullptr) {
      GEOS_finish_r(handle);
    }
  }

  GeosContext(const GeosContext&) = delete;
  GeosContext& operator=(const GeosContext&) = delete;

  explicit operator bool() const { return handle != nullptr; }

 private:
  static void ignore_message(const char* /*fmt*/, ...) {}
};

struct XyKey {
  std::uint64_t xbits = 0;
  std::uint64_t ybits = 0;
  bool operator==(const XyKey& other) const = default;
};

struct XyKeyHash {
  std::size_t operator()(const XyKey& key) const noexcept {
    return std::hash<std::uint64_t>{}(key.xbits) ^
           (std::hash<std::uint64_t>{}(key.ybits) << 1);
  }
};

XyKey make_xy_key(double x, double y) {
  XyKey key;
  std::memcpy(&key.xbits, &x, sizeof(x));
  std::memcpy(&key.ybits, &y, sizeof(y));
  return key;
}

class VertexIndexMap {
 public:
  VertexIndexMap(const base::Vector3* points, int count)
      : points_(points), count_(count) {
    exact_.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      exact_.emplace(make_xy_key(static_cast<double>(points[i].x),
                                 static_cast<double>(points[i].y)),
                     i);
    }
  }

  int lookup(double x, double y) const {
    const auto it = exact_.find(make_xy_key(x, y));
    if (it != exact_.end()) {
      return it->second;
    }
    for (int i = 0; i < count_; ++i) {
      if (xy_eq(static_cast<float>(x), points_[i].x) &&
          xy_eq(static_cast<float>(y), points_[i].y)) {
        return i;
      }
    }
    return -1;
  }

 private:
  const base::Vector3* points_ = nullptr;
  int count_ = 0;
  std::unordered_map<XyKey, int, XyKeyHash> exact_;
};

void destroy_geoms(GEOSContextHandle_t ctx, std::vector<GEOSGeometry*>& geoms) {
  for (GEOSGeometry* geom : geoms) {
    if (geom != nullptr) {
      GEOSGeom_destroy_r(ctx, geom);
    }
  }
  geoms.clear();
}

bool extract_triangle(GEOSContextHandle_t ctx,
                      const GEOSGeometry* poly,
                      const VertexIndexMap& index_map,
                      IndexedTriangle& out) {
  if (poly == nullptr || GEOSGeomTypeId_r(ctx, poly) != GEOS_POLYGON) {
    return false;
  }
  const GEOSGeometry* ring = GEOSGetExteriorRing_r(ctx, poly);
  if (ring == nullptr) {
    return false;
  }
  const GEOSCoordSequence* coords = GEOSGeom_getCoordSeq_r(ctx, ring);
  if (coords == nullptr) {
    return false;
  }
  unsigned int size = 0;
  if (GEOSCoordSeq_getSize_r(ctx, coords, &size) == 0 || size < 3) {
    return false;
  }

  int idx[3] = {-1, -1, -1};
  double xs[3] = {};
  double ys[3] = {};
  for (unsigned int i = 0; i < 3; ++i) {
    if (GEOSCoordSeq_getXY_r(ctx, coords, i, &xs[i], &ys[i]) == 0) {
      return false;
    }
    idx[i] = index_map.lookup(xs[i], ys[i]);
    if (idx[i] < 0) {
      return false;
    }
  }
  if (idx[0] == idx[1] || idx[1] == idx[2] || idx[2] == idx[0]) {
    return false;
  }
  const double cross =
      (xs[1] - xs[0]) * (ys[2] - ys[0]) - (ys[1] - ys[0]) * (xs[2] - xs[0]);
  if (cross < 0.0) {
    std::swap(idx[1], idx[2]);
  }
  out.a = idx[0];
  out.b = idx[1];
  out.c = idx[2];
  return true;
}

bool append_triangles_from(GEOSContextHandle_t ctx,
                           const GEOSGeometry* geom,
                           const VertexIndexMap& index_map,
                           std::vector<IndexedTriangle>& triangles) {
  if (geom == nullptr) {
    return false;
  }
  const int type = GEOSGeomTypeId_r(ctx, geom);
  if (type == GEOS_POLYGON) {
    IndexedTriangle tri;
    if (!extract_triangle(ctx, geom, index_map, tri)) {
      return false;
    }
    triangles.push_back(tri);
    return true;
  }
  if (type != GEOS_GEOMETRYCOLLECTION && type != GEOS_MULTIPOLYGON) {
    return false;
  }
  const int n = GEOSGetNumGeometries_r(ctx, geom);
  if (n < 0) {
    return false;
  }
  bool mapped_any = (n == 0);
  for (int i = 0; i < n; ++i) {
    const GEOSGeometry* child = GEOSGetGeometryN_r(ctx, geom, i);
    IndexedTriangle tri;
    if (extract_triangle(ctx, child, index_map, tri)) {
      triangles.push_back(tri);
      mapped_any = true;
    }
  }
  return mapped_any || n == 0;
}

bool geos_delaunay(const base::Vector3* points,
                   int count,
                   std::vector<IndexedTriangle>& triangles) {
  triangles.clear();
  if (points == nullptr || count < 3) {
    return false;
  }

  GeosContext ctx;
  if (!ctx) {
    return false;
  }

  std::vector<GEOSGeometry*> sites(static_cast<std::size_t>(count), nullptr);
  for (int i = 0; i < count; ++i) {
    sites[static_cast<std::size_t>(i)] = GEOSGeom_createPointFromXY_r(
        ctx.handle, static_cast<double>(points[i].x),
        static_cast<double>(points[i].y));
    if (sites[static_cast<std::size_t>(i)] == nullptr) {
      destroy_geoms(ctx.handle, sites);
      return false;
    }
  }

  GEOSGeometry* multipoint = GEOSGeom_createCollection_r(
      ctx.handle, GEOS_MULTIPOINT, sites.data(),
      static_cast<unsigned int>(count));
  if (multipoint == nullptr) {
    destroy_geoms(ctx.handle, sites);
    return false;
  }

  GEOSGeometry* mesh =
      GEOSDelaunayTriangulation_r(ctx.handle, multipoint, 0.0, 0);
  GEOSGeom_destroy_r(ctx.handle, multipoint);
  if (mesh == nullptr) {
    return false;
  }

  const VertexIndexMap index_map(points, count);
  const bool ok = append_triangles_from(ctx.handle, mesh, index_map, triangles);
  GEOSGeom_destroy_r(ctx.handle, mesh);
  return ok;
}

bool geos_delaunay_constrained(const base::Vector3* points,
                               int count,
                               std::vector<IndexedTriangle>& triangles) {
  triangles.clear();
  if (points == nullptr || count < 3) {
    return false;
  }

  GeosContext ctx;
  if (!ctx) {
    return false;
  }

  const bool closed = xy_eq(points[0].x, points[count - 1].x) &&
                      xy_eq(points[0].y, points[count - 1].y);
  const unsigned int seq_size =
      static_cast<unsigned int>(closed ? count : count + 1);

  GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx.handle, seq_size, 2);
  if (seq == nullptr) {
    return false;
  }
  for (int i = 0; i < count; ++i) {
    if (GEOSCoordSeq_setXY_r(ctx.handle, seq, static_cast<unsigned int>(i),
                             static_cast<double>(points[i].x),
                             static_cast<double>(points[i].y)) == 0) {
      GEOSCoordSeq_destroy_r(ctx.handle, seq);
      return false;
    }
  }
  if (!closed) {
    if (GEOSCoordSeq_setXY_r(ctx.handle, seq, seq_size - 1,
                             static_cast<double>(points[0].x),
                             static_cast<double>(points[0].y)) == 0) {
      GEOSCoordSeq_destroy_r(ctx.handle, seq);
      return false;
    }
  }

  GEOSGeometry* ring = GEOSGeom_createLinearRing_r(ctx.handle, seq);
  if (ring == nullptr) {
    GEOSCoordSeq_destroy_r(ctx.handle, seq);
    return false;
  }
  GEOSGeometry* poly = GEOSGeom_createPolygon_r(ctx.handle, ring, nullptr, 0);
  if (poly == nullptr) {
    GEOSGeom_destroy_r(ctx.handle, ring);
    return false;
  }

  GEOSGeometry* mesh = GEOSConstrainedDelaunayTriangulation_r(ctx.handle, poly);
  GEOSGeom_destroy_r(ctx.handle, poly);
  if (mesh == nullptr) {
    return false;
  }

  const VertexIndexMap index_map(points, count);
  const bool ok = append_triangles_from(ctx.handle, mesh, index_map, triangles);
  GEOSGeom_destroy_r(ctx.handle, mesh);
  return ok;
}

bool fill_ogr_tin(OGRTriangulatedSurface* mesh,
                  const base::Vector3* points,
                  int count,
                  const std::vector<IndexedTriangle>& triangles) {
  std::vector<Vertex3> verts(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    verts[static_cast<size_t>(i)] =
        Vertex3(static_cast<double>(points[i].x),
                static_cast<double>(points[i].y),
                static_cast<double>(points[i].z));
  }
  return fill_indexed_tin(mesh, verts, triangles);
}

}  // namespace detail

bool delaunay_triangles(std::vector<IndexedTriangle>& triangles,
                        const base::Vector3* points,
                        int count) {
  if (count < 3) {
    triangles.clear();
    return true;
  }
  return detail::geos_delaunay(points, count, triangles);
}

bool delaunay(OGRTriangulatedSurface* mesh,
              const base::Vector3* points,
              int count) {
  if (mesh == nullptr || points == nullptr || count < 1) {
    return false;
  }
  std::vector<IndexedTriangle> triangles;
  if (!delaunay_triangles(triangles, points, count)) {
    return false;
  }
  return detail::fill_ogr_tin(mesh, points, count, triangles);
}

bool delaunay(OGRTriangulatedSurface* mesh,
              const std::vector<base::Vector3>& points) {
  if (points.empty()) {
    return false;
  }
  return delaunay(mesh, points.data(), static_cast<int>(points.size()));
}

bool delaunay_constrained(std::vector<IndexedTriangle>& triangles,
                          const base::Vector3* points,
                          int count) {
  return detail::geos_delaunay_constrained(points, count, triangles);
}

}  // namespace geo
