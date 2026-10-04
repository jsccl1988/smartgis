// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/feature/feature_mesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/tin/delaunay.h"
#include "ogr_geometry.h"

namespace scenic {
namespace detail {
namespace {

constexpr int kMaxTessRingVerts = 64;

int downsample_ring(const OGRRawPoint* src, int n,
                    std::vector<OGRRawPoint>* dst, int max_verts) {
  if (!src || !dst || n < 3 || max_verts < 3) {
    return 0;
  }
  dst->clear();
  if (n <= max_verts) {
    dst->assign(src, src + n);
    return n;
  }
  const int step = (n + max_verts - 2) / (max_verts - 1);
  dst->reserve(static_cast<size_t>(max_verts));
  for (int i = 0; i < n && static_cast<int>(dst->size()) < max_verts - 1;
       i += step) {
    dst->push_back(src[i]);
  }
  dst->push_back(src[n - 1]);
  return static_cast<int>(dst->size());
}

float height_at(FeatureHeightSampleFn fn, void* user, double x, double y) {
  if (fn) {
    return fn(x, y, user);
  }
  return 0.f;
}

void emit_map_vertex(FeatureMesh* mesh, double x, double y, const FeatureRgb& c,
                     FeatureHeightSampleFn height_fn, void* height_user) {
  FeatureVertex v;
  v.has_color = true;
  v.r = c.r;
  v.g = c.g;
  v.b = c.b;
  v.a = 1.f;
  const float h = height_at(height_fn, height_user, x, y);
  if (height_fn) {
    const float eps = 0.12f;
    const float hx0 = height_at(height_fn, height_user, x - eps, y);
    const float hx1 = height_at(height_fn, height_user, x + eps, y);
    const float hy0 = height_at(height_fn, height_user, x, y - eps);
    const float hy1 = height_at(height_fn, height_user, x, y + eps);
    float nx = -(hx0 - hx1);
    float ny = 2.f * eps;
    float nz = hy0 - hy1;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) {
      nx /= len;
      ny /= len;
      nz /= len;
    } else {
      nx = 0.f;
      ny = 1.f;
      nz = 0.f;
    }
    v.has_normal = true;
    v.nx = nx;
    v.ny = ny;
    v.nz = nz;
    mesh->has_normals = true;
  }
  v.x = static_cast<float>(-x);
  v.y = h;
  v.z = static_cast<float>(y);
  mesh->has_colors = true;
  mesh->vertices.push_back(v);
}

bool tess_polygon(OGRPolygon* poly, const FeatureRgb& fill,
                  FeatureHeightSampleFn height_fn, void* height_user,
                  FeatureMesh* out) {
  if (!poly || !out) {
    return false;
  }
  OGRLinearRing* ring = poly->getExteriorRing();
  if (!ring) {
    return false;
  }
  int n_points = ring->getNumPoints();
  if (n_points < 3) {
    return false;
  }
  std::vector<OGRRawPoint> raw(static_cast<size_t>(n_points));
  for (int i = 0; i < n_points; ++i) {
    raw[static_cast<size_t>(i)].x = ring->getX(i);
    raw[static_cast<size_t>(i)].y = ring->getY(i);
  }
  if (n_points >= 4 && raw[0].x == raw[static_cast<size_t>(n_points - 1)].x &&
      raw[0].y == raw[static_cast<size_t>(n_points - 1)].y) {
    --n_points;
    raw.resize(static_cast<size_t>(n_points));
  }
  if (n_points < 3) {
    return false;
  }
  std::vector<OGRRawPoint> slim;
  const int slim_n =
      downsample_ring(raw.data(), n_points, &slim, kMaxTessRingVerts);
  if (slim_n < 3) {
    return false;
  }
  raw = std::move(slim);
  n_points = slim_n;

  std::vector<base::Vector3> ring_xyz(static_cast<std::size_t>(n_points));
  for (int i = 0; i < n_points; ++i) {
    ring_xyz[static_cast<std::size_t>(i)].x =
        static_cast<float>(raw[static_cast<std::size_t>(i)].x);
    ring_xyz[static_cast<std::size_t>(i)].y =
        static_cast<float>(raw[static_cast<std::size_t>(i)].y);
    ring_xyz[static_cast<std::size_t>(i)].z = 0.f;
  }
  std::vector<geo::IndexedTriangle> tris;
  if (!geo::delaunay_constrained(tris, ring_xyz.data(), n_points)) {
    return false;
  }
  std::vector<geo::IndexedTriangle> in_range;
  in_range.reserve(tris.size());
  for (const geo::IndexedTriangle& t : tris) {
    if (t.a >= 0 && t.b >= 0 && t.c >= 0 && t.a < n_points && t.b < n_points &&
        t.c < n_points) {
      in_range.push_back(t);
    }
  }
  if (in_range.empty()) {
    return false;
  }

  out->prim = FeaturePrim::kTriangles;
  out->indexed = true;
  const int base = static_cast<int>(out->vertices.size());
  for (int i = 0; i < n_points; ++i) {
    emit_map_vertex(out, raw[static_cast<size_t>(i)].x,
                    raw[static_cast<size_t>(i)].y, fill, height_fn,
                    height_user);
  }
  for (const geo::IndexedTriangle& t : in_range) {
    out->indices.push_back(static_cast<std::uint32_t>(base + t.a));
    out->indices.push_back(static_cast<std::uint32_t>(base + t.b));
    out->indices.push_back(static_cast<std::uint32_t>(base + t.c));
  }
  return true;
}

bool tess_multi_polygon(OGRMultiPolygon* multi, const FeatureRgb& fill,
                        FeatureHeightSampleFn height_fn, void* height_user,
                        FeatureMesh* out) {
  if (!multi || !out) {
    return false;
  }
  if (multi->getNumGeometries() == 1) {
    return tess_polygon(static_cast<OGRPolygon*>(multi->getGeometryRef(0)),
                        fill, height_fn, height_user, out);
  }

  const int ngeom = multi->getNumGeometries();
  double max_part_area = 0.0;
  for (int gi = 0; gi < ngeom; ++gi) {
    OGRPolygon* poly = static_cast<OGRPolygon*>(multi->getGeometryRef(gi));
    OGRLinearRing* ring = poly ? poly->getExteriorRing() : nullptr;
    if (!ring || ring->getNumPoints() < 3) {
      continue;
    }
    OGREnvelope env;
    ring->getEnvelope(&env);
    const double area = (env.MaxX - env.MinX) * (env.MaxY - env.MinY);
    if (area > max_part_area) {
      max_part_area = area;
    }
  }

  FeatureMesh local;
  local.prim = FeaturePrim::kTriangles;
  local.indexed = true;
  for (int gi = 0; gi < ngeom; ++gi) {
    OGRPolygon* poly = static_cast<OGRPolygon*>(multi->getGeometryRef(gi));
    if (!poly) {
      continue;
    }
    OGRLinearRing* ring = poly->getExteriorRing();
    if (!ring || ring->getNumPoints() < 3) {
      continue;
    }
    OGREnvelope env;
    ring->getEnvelope(&env);
    const double area = (env.MaxX - env.MinX) * (env.MaxY - env.MinY);
    if (max_part_area > 0.0 && area < max_part_area * 0.02) {
      continue;
    }
    FeatureMesh part;
    if (!tess_polygon(poly, fill, height_fn, height_user, &part)) {
      continue;
    }
    const int base = static_cast<int>(local.vertices.size());
    local.vertices.insert(local.vertices.end(), part.vertices.begin(),
                          part.vertices.end());
    if (part.has_normals) {
      local.has_normals = true;
    }
    if (part.has_colors) {
      local.has_colors = true;
    }
    for (std::uint32_t ix : part.indices) {
      local.indices.push_back(static_cast<std::uint32_t>(base) + ix);
    }
  }
  if (local.vertices.empty() || local.indices.empty()) {
    return false;
  }
  *out = std::move(local);
  return true;
}

}  // namespace

bool tess_map_geometry(const OGRGeometry& geom, const FeatureRgb& stroke,
                       const FeatureRgb& fill,
                       FeatureHeightSampleFn height_fn, void* height_user,
                       FeatureMesh* out) {
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
      emit_map_vertex(out, p->getX(), p->getY(), stroke, height_fn,
                      height_user);
      return true;
    }
    case wkbMultiPoint: {
      const auto* mp = static_cast<const OGRMultiPoint*>(&geom);
      out->prim = FeaturePrim::kPoints;
      for (int i = 0; i < mp->getNumGeometries(); ++i) {
        const auto* p = static_cast<const OGRPoint*>(mp->getGeometryRef(i));
        emit_map_vertex(out, p->getX(), p->getY(), stroke, height_fn,
                        height_user);
      }
      return !out->vertices.empty();
    }
    case wkbLineString:
    case wkbLinearRing: {
      const auto* line = static_cast<const OGRLineString*>(&geom);
      out->prim = FeaturePrim::kLineStrip;
      for (int i = 0; i < line->getNumPoints(); ++i) {
        emit_map_vertex(out, line->getX(i), line->getY(i), stroke, height_fn,
                        height_user);
      }
      return out->vertices.size() >= 2;
    }
    case wkbMultiLineString: {
      const auto* ml = static_cast<const OGRMultiLineString*>(&geom);
      out->prim = FeaturePrim::kLineStrip;
      for (int i = 0; i < ml->getNumGeometries(); ++i) {
        const auto* line =
            static_cast<const OGRLineString*>(ml->getGeometryRef(i));
        for (int k = 0; k < line->getNumPoints(); ++k) {
          emit_map_vertex(out, line->getX(k), line->getY(k), stroke, height_fn,
                          height_user);
        }
      }
      return out->vertices.size() >= 2;
    }
    case wkbPolygon:
      return tess_polygon(const_cast<OGRPolygon*>(static_cast<const OGRPolygon*>(
                              &geom)),
                          fill, height_fn, height_user, out);
    case wkbMultiPolygon:
      return tess_multi_polygon(
          const_cast<OGRMultiPolygon*>(
              static_cast<const OGRMultiPolygon*>(&geom)),
          fill, height_fn, height_user, out);
    default:
      return false;
  }
}

}  // namespace detail
}  // namespace scenic
