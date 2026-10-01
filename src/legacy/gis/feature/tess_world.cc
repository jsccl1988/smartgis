// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/feature/mesh.h"

#include "base/math/math.h"
#include "gis/kernel/geo/mesh/geometry.h"
#include "legacy/core/types/types.h"
#include "ogr_geometry.h"

namespace render {
namespace {

void emit_world_xyz(FeatureMesh* mesh, double x, double y, double z) {
  FeatureVertex v;
  // Leftover 3D: OGR (X,Y,Z) → VB (X, Z, Y).
  v.x = static_cast<float>(x);
  v.y = static_cast<float>(z);
  v.z = static_cast<float>(y);
  mesh->vertices.push_back(v);
}

}  // namespace

bool tess_world_geometry(const OGRGeometry& geom, FeatureMesh* out) {
  if (!out) {
    return false;
  }
  out->vertices.clear();
  out->indices.clear();
  out->indexed = false;
  out->has_normals = false;
  out->has_colors = false;

  switch (wkbFlatten(geom.getGeometryType())) {
    case wkbPoint: {
      const auto* p = static_cast<const OGRPoint*>(&geom);
      out->prim = FeaturePrim::kPoints;
      emit_world_xyz(out, p->getX(), p->getY(), p->getZ());
      return true;
    }
    case wkbMultiPoint: {
      const auto* mp = static_cast<const OGRMultiPoint*>(&geom);
      out->prim = FeaturePrim::kPoints;
      for (int i = 0; i < mp->getNumGeometries(); ++i) {
        const auto* p = static_cast<const OGRPoint*>(mp->getGeometryRef(i));
        emit_world_xyz(out, p->getX(), p->getY(), p->getZ());
      }
      return !out->vertices.empty();
    }
    case wkbLineString:
    case wkbLinearRing: {
      const auto* line = static_cast<const OGRLineString*>(&geom);
      out->prim = FeaturePrim::kLineStrip;
      for (int i = 0; i < line->getNumPoints(); ++i) {
        emit_world_xyz(out, line->getX(i), line->getY(i), line->getZ(i));
      }
      return out->vertices.size() >= 2;
    }
    case wkbMultiLineString:
      // Leftover Create3DMultiLineStringVB was a no-op.
      return false;
    default:
      return false;
  }
}

bool tess_3d_surface(const geo::Surface3d& surf, FeatureMesh* out) {
  if (!out || surf.get_point_count() <= 0) {
    return false;
  }
  out->vertices.clear();
  out->indices.clear();
  out->prim = FeaturePrim::kTriangles;
  out->indexed = true;
  out->has_normals = true;
  out->has_colors = false;

  const int npts = surf.get_point_count();
  out->vertices.resize(static_cast<size_t>(npts));
  for (int i = 0; i < npts; ++i) {
    OGRPoint point = surf.get_point(i);
    FeatureVertex& v = out->vertices[static_cast<size_t>(i)];
    v.x = static_cast<float>(point.getX());
    v.y = static_cast<float>(point.getZ());
    v.z = static_cast<float>(point.getY());
    v.has_normal = true;
  }

  const int ntri = surf.get_triangle_count();
  out->indices.reserve(static_cast<size_t>(ntri) * 3);
  std::vector<Vector4> normals(static_cast<size_t>(npts));
  for (int i = 0; i < ntri; ++i) {
    base::SmtTriangle tri = surf.get_triangle(i);
    out->indices.push_back(static_cast<std::uint32_t>(tri.a));
    out->indices.push_back(static_cast<std::uint32_t>(tri.b));
    out->indices.push_back(static_cast<std::uint32_t>(tri.c));
    OGRPoint p1 = surf.get_point(tri.a);
    OGRPoint p2 = surf.get_point(tri.b);
    OGRPoint p3 = surf.get_point(tri.c);
    Vector4 V1(p1.getX(), p1.getY(), p1.getZ());
    Vector4 V2(p2.getX(), p2.getY(), p2.getZ());
    Vector4 V3(p3.getX(), p3.getY(), p3.getZ());
    Vector4 nor = triangle_normal(V1, V2, V3);
    normals[static_cast<size_t>(tri.a)] += nor;
    normals[static_cast<size_t>(tri.b)] += nor;
    normals[static_cast<size_t>(tri.c)] += nor;
  }
  for (int i = 0; i < npts; ++i) {
    normals[static_cast<size_t>(i)].normalize();
    FeatureVertex& v = out->vertices[static_cast<size_t>(i)];
    // Match leftover: Normal(nx, nz, ny) in VB space.
    v.nx = normals[static_cast<size_t>(i)].x;
    v.ny = normals[static_cast<size_t>(i)].z;
    v.nz = normals[static_cast<size_t>(i)].y;
  }
  return !out->indices.empty();
}

}  // namespace render
