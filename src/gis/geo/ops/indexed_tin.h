// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_INDEXED_TIN_H_
#define GIS_GEO_OPS_INDEXED_TIN_H_

#include "ogr_geometry.h"

#include <span>

namespace geo {

// Double XYZ staging for indexed TIN write (not leftover dbf3DPoint).
struct Vertex3 {
  double x = 0;
  double y = 0;
  double z = 0;
  Vertex3() = default;
  Vertex3(double x_in, double y_in, double z_in)
      : x(x_in), y(y_in), z(z_in) {}
};

// Corner indices into a vertex array (XY Delaunay / indexed TIN write).
struct IndexedTriangle {
  int a = -1;
  int b = -1;
  int c = -1;
};

// Indexed TIN write onto OGRTriangulatedSurface (wkbTIN + OGRTriangle).
// Staging is Vertex3 / IndexedTriangle; Delaunay stays in geo::delaunay,
// buffer in geo::buffer.

inline OGRPoint make_xyz_point(const Vertex3& p) {
  OGRPoint pt(p.x, p.y, p.z);
  pt.setCoordinateDimension(3);
  return pt;
}

inline bool triangle_indices_in_range(const IndexedTriangle& tri,
                                      int n_points) {
  return tri.a >= 0 && tri.b >= 0 && tri.c >= 0 && tri.a < n_points &&
         tri.b < n_points && tri.c < n_points;
}

// Append one OGRTriangle (closed ring) onto a native TIN.
inline bool add_patch(OGRTriangulatedSurface* mesh,
                      const OGRPoint& a,
                      const OGRPoint& b,
                      const OGRPoint& c) {
  if (mesh == nullptr) {
    return false;
  }
  OGRTriangle patch(a, b, c);
  if (OGRLinearRing* ring = patch.getExteriorRing()) {
    if (!ring->get_IsClosed()) {
      ring->closeRings();
    }
  }
  return mesh->addGeometry(&patch) == OGRERR_NONE;
}

inline bool add_triangle(OGRTriangulatedSurface* mesh,
                         std::span<const Vertex3> vertices,
                         const IndexedTriangle& tri) {
  if (mesh == nullptr ||
      !triangle_indices_in_range(tri, static_cast<int>(vertices.size()))) {
    return false;
  }
  const OGRPoint pa = make_xyz_point(vertices[static_cast<size_t>(tri.a)]);
  const OGRPoint pb = make_xyz_point(vertices[static_cast<size_t>(tri.b)]);
  const OGRPoint pc = make_xyz_point(vertices[static_cast<size_t>(tri.c)]);
  return add_patch(mesh, pa, pb, pc);
}

inline bool add_triangle_collection(OGRTriangulatedSurface* mesh,
                                    std::span<const Vertex3> vertices,
                                    std::span<const IndexedTriangle> tris) {
  if (mesh == nullptr || tris.empty()) {
    return false;
  }
  for (const IndexedTriangle& tri : tris) {
    if (!add_triangle(mesh, vertices, tri)) {
      return false;
    }
  }
  return true;
}

// Replace dst with faces from indexed vertices. Empty tris leaves an empty TIN
// (staging is not stored on OGR).
inline bool fill_indexed_tin(OGRTriangulatedSurface* mesh,
                             std::span<const Vertex3> vertices,
                             std::span<const IndexedTriangle> tris) {
  if (mesh == nullptr) {
    return false;
  }
  mesh->empty();
  if (tris.empty()) {
    return true;
  }
  return add_triangle_collection(mesh, vertices, tris);
}

// First three ring vertices as a triangle. Uses getPoint so instance Z
// is kept (OGC coordinateDimension stays on the OGR points).
inline void append_triangle_from_ring(OGRTriangulatedSurface* mesh,
                                      const OGRLinearRing* ring) {
  if (!mesh || !ring || ring->getNumPoints() < 3) {
    return;
  }
  OGRPoint p0;
  OGRPoint p1;
  OGRPoint p2;
  ring->getPoint(0, &p0);
  ring->getPoint(1, &p1);
  ring->getPoint(2, &p2);
  add_patch(mesh, p0, p1, p2);
}

inline void append_triangle_from_polygon(OGRTriangulatedSurface* mesh,
                                         const OGRPolygon* poly) {
  if (!poly) {
    return;
  }
  append_triangle_from_ring(mesh, poly->getExteriorRing());
}

inline bool tin_patch_points(const OGRTriangulatedSurface& mesh,
                             int index,
                             OGRPoint* a,
                             OGRPoint* b,
                             OGRPoint* c) {
  if (!a || !b || !c || index < 0) {
    return false;
  }
  auto* writable = const_cast<OGRTriangulatedSurface*>(&mesh);
  if (index >= writable->getNumGeometries()) {
    return false;
  }
  OGRPolygon* patch = writable->getGeometryRef(index);
  if (!patch) {
    return false;
  }
  OGRLinearRing* ring = patch->getExteriorRing();
  if (!ring || ring->getNumPoints() < 3) {
    return false;
  }
  ring->getPoint(0, a);
  ring->getPoint(1, b);
  ring->getPoint(2, c);
  return true;
}

// Returns false when geom is not Polygon / MultiPolygon / TIN, or empty.
// OGRTriangulatedSurface is an OGRSurface (not GeometryCollection).
inline bool fill_tin_from_ogr(OGRTriangulatedSurface* dst,
                              const OGRGeometry& geom) {
  if (dst == nullptr) {
    return false;
  }
  dst->empty();
  if (auto* ts = dynamic_cast<const OGRTriangulatedSurface*>(&geom)) {
    auto* writable = const_cast<OGRTriangulatedSurface*>(ts);
    const int n = writable->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      dst->addGeometry(writable->getGeometryRef(i));
    }
    return dst->IsEmpty() == FALSE;
  }
  if (auto* ps = dynamic_cast<const OGRPolyhedralSurface*>(&geom)) {
    const int n = ps->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      append_triangle_from_polygon(dst, ps->getGeometryRef(i));
    }
  } else if (auto* multi = dynamic_cast<const OGRMultiPolygon*>(&geom)) {
    const int n = multi->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      append_triangle_from_polygon(dst, multi->getGeometryRef(i)->toPolygon());
    }
  } else if (auto* poly = dynamic_cast<const OGRPolygon*>(&geom)) {
    append_triangle_from_polygon(dst, poly);
  } else {
    return false;
  }
  return dst->getNumGeometries() >= 1;
}

}  // namespace geo

#endif  // GIS_GEO_OPS_INDEXED_TIN_H_
