// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/detail/feature_mesh.h"

#include <vector>

#include "base/math/math.h"
#include "gis/geo/ops/indexed_tin.h"
#include "scenic/detail/geom.h"
#include "ogr_geometry.h"

namespace scenic {
namespace detail {
namespace {

struct CachedPoint {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

void emit_world_xyz(FeatureMesh* mesh, double x, double y, double z) {
  FeatureVertex v;
  // Leftover 3D: OGR (X,Y,Z) �?VB (X, Z, Y).
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

bool tess_3d_surface(const OGRTriangulatedSurface& surf, FeatureMesh* out) {
  auto* writable = const_cast<OGRTriangulatedSurface*>(&surf);
  const int ntri = writable->getNumGeometries();
  if (!out || ntri < 1) {
    return false;
  }
  out->vertices.clear();
  out->indices.clear();
  out->prim = FeaturePrim::kTriangles;
  out->indexed = true;
  out->has_normals = true;
  out->has_colors = false;

  const int npts = ntri * 3;
  std::vector<CachedPoint> pts(static_cast<size_t>(npts));
  out->vertices.resize(static_cast<size_t>(npts));
  out->indices.reserve(static_cast<size_t>(npts));
  std::vector<Vector4> normals(static_cast<size_t>(npts));
  int live = 0;
  for (int i = 0; i < ntri; ++i) {
    OGRPoint pa;
    OGRPoint pb;
    OGRPoint pc;
    if (!geo::tin_patch_points(surf, i, &pa, &pb, &pc)) {
      continue;
    }
    const int ia = live++;
    const int ib = live++;
    const int ic = live++;
    auto store = [&](int idx, const OGRPoint& p) {
      CachedPoint& c = pts[static_cast<size_t>(idx)];
      c.x = static_cast<float>(p.getX());
      c.y = static_cast<float>(p.getY());
      c.z = static_cast<float>(p.getZ());
      FeatureVertex& v = out->vertices[static_cast<size_t>(idx)];
      v.x = c.x;
      v.y = c.z;
      v.z = c.y;
      v.has_normal = true;
    };
    store(ia, pa);
    store(ib, pb);
    store(ic, pc);
    out->indices.push_back(static_cast<std::uint32_t>(ia));
    out->indices.push_back(static_cast<std::uint32_t>(ib));
    out->indices.push_back(static_cast<std::uint32_t>(ic));
    const CachedPoint& a = pts[static_cast<size_t>(ia)];
    const CachedPoint& b = pts[static_cast<size_t>(ib)];
    const CachedPoint& c = pts[static_cast<size_t>(ic)];
    const Vector4 nor =
        triangle_normal(Vector4(a.x, a.y, a.z), Vector4(b.x, b.y, b.z),
                        Vector4(c.x, c.y, c.z));
    normals[static_cast<size_t>(ia)] += nor;
    normals[static_cast<size_t>(ib)] += nor;
    normals[static_cast<size_t>(ic)] += nor;
  }
  out->vertices.resize(static_cast<size_t>(live));
  normals.resize(static_cast<size_t>(live));
  for (int i = 0; i < live; ++i) {
    normals[static_cast<size_t>(i)].normalize();
    FeatureVertex& v = out->vertices[static_cast<size_t>(i)];
    v.nx = normals[static_cast<size_t>(i)].x;
    v.ny = normals[static_cast<size_t>(i)].z;
    v.nz = normals[static_cast<size_t>(i)].y;
  }
  return !out->indices.empty();
}

}  // namespace detail
}  // namespace scenic
