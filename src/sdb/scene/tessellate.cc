// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/tessellate.h"

#include "3dgeometry.h"
#include "geometry.h"
#include "layer.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "ogrsf_frmts.h"

#include <cmath>

namespace sdb {
namespace scene {
namespace {

void append_xyz(TessMesh& mesh, float x, float y, float z) {
  mesh.positions.push_back(x);
  mesh.positions.push_back(y);
  mesh.positions.push_back(z);
}

uint32_t vert_count(const TessMesh& mesh) {
  return static_cast<uint32_t>(mesh.positions.size() / 3);
}

void tessellate_point_xy(double x, double y, double z, TessMesh& out) {
  const float s = 0.05f;
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(x), static_cast<float>(y) + s,
             static_cast<float>(z));
  append_xyz(out, static_cast<float>(x) - s, static_cast<float>(y) - s,
             static_cast<float>(z));
  append_xyz(out, static_cast<float>(x) + s, static_cast<float>(y) - s,
             static_cast<float>(z));
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
}

void tessellate_segment(double ax, double ay, double az, double bx, double by,
                        double bz, TessMesh& out) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len = std::sqrt(dx * dx + dy * dy);
  double nx = 0;
  double ny = 0.05;
  if (len > 1e-9) {
    nx = (-dy / len) * 0.05;
    ny = (dx / len) * 0.05;
  }
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(ax + nx), static_cast<float>(ay + ny),
             static_cast<float>(az));
  append_xyz(out, static_cast<float>(ax - nx), static_cast<float>(ay - ny),
             static_cast<float>(az));
  append_xyz(out, static_cast<float>(bx + nx), static_cast<float>(by + ny),
             static_cast<float>(bz));
  append_xyz(out, static_cast<float>(bx - nx), static_cast<float>(by - ny),
             static_cast<float>(bz));
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 3);
  out.indices.push_back(base + 2);
}

void tessellate_line(const Smt_Geo::SmtLineString* line, TessMesh& out) {
  if (!line) {
    return;
  }
  const int n = line->GetNumPoints();
  for (int i = 0; i + 1 < n; ++i) {
    tessellate_segment(line->GetX(i), line->GetY(i), 0, line->GetX(i + 1),
                       line->GetY(i + 1), 0, out);
  }
}

void tessellate_ring_fan(const Smt_Geo::SmtLinearRing* ring, TessMesh& out) {
  if (!ring) {
    return;
  }
  int n = ring->GetNumPoints();
  if (n >= 2 && ring->GetX(0) == ring->GetX(n - 1) &&
      ring->GetY(0) == ring->GetY(n - 1)) {
    --n;
  }
  if (n < 3) {
    return;
  }
  const uint32_t base = vert_count(out);
  for (int i = 0; i < n; ++i) {
    append_xyz(out, static_cast<float>(ring->GetX(i)),
               static_cast<float>(ring->GetY(i)), 0);
  }
  for (int i = 1; i + 1 < n; ++i) {
    out.indices.push_back(base);
    out.indices.push_back(base + static_cast<uint32_t>(i));
    out.indices.push_back(base + static_cast<uint32_t>(i + 1));
  }
}

void tessellate_polygon(const Smt_Geo::SmtPolygon* poly, TessMesh& out) {
  if (!poly) {
    return;
  }
  tessellate_ring_fan(poly->GetExteriorRing(), out);
}

bool tessellate_geom_into(const Smt_Geo::SmtGeometry* geom, TessMesh& out) {
  if (!geom) {
    return false;
  }
  switch (geom->GetGeometryType()) {
    case Smt_Geo::GTPoint: {
      const auto* p = static_cast<const Smt_Geo::SmtPoint*>(geom);
      tessellate_point_xy(p->GetX(), p->GetY(), 0, out);
      return true;
    }
    case Smt_Geo::GTLineString:
    case Smt_Geo::GTSpline:
    case Smt_Geo::GTLinearRing:
      tessellate_line(static_cast<const Smt_Geo::SmtLineString*>(geom), out);
      return true;
    case Smt_Geo::GTPolygon:
      tessellate_polygon(static_cast<const Smt_Geo::SmtPolygon*>(geom), out);
      return true;
    case Smt_Geo::GTMultiPoint:
    case Smt_Geo::GTMultiLineString:
    case Smt_Geo::GTMultiPolygon:
    case Smt_Geo::GTGeometryCollection: {
      const auto* col =
          static_cast<const Smt_Geo::SmtGeometryCollection*>(geom);
      const int n = col->GetNumGeometries();
      bool any = false;
      for (int i = 0; i < n; ++i) {
        if (tessellate_geom_into(col->GetGeometryRef(i), out)) {
          any = true;
        }
      }
      return any;
    }
    default:
      return false;
  }
}

}  // namespace

bool tessellate_geometry(const Smt_Geo::SmtGeometry* geom, TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  if (!tessellate_geom_into(geom, out)) {
    return false;
  }
  return !out.indices.empty();
}

bool tessellate_geoms(const Smt_Geo::SmtGeometry* const* geoms, size_t count,
                      TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  if (!geoms || count == 0) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    tessellate_geom_into(geoms[i], out);
  }
  return !out.indices.empty();
}

bool tessellate_layer(OGRLayer* layer, TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  if (!layer) {
    return false;
  }
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    Smt_Geo::SmtGeometry* geom =
        sdb::datasource::decode_ogr_geometry(feat, Smt_GIS::SmtFtUnknown);
    tessellate_geom_into(geom, out);
    delete geom;
    OGRFeature::DestroyFeature(feat);
  }
  return !out.indices.empty();
}

bool tessellate_3d_geometry(const Smt_3DGeo::Smt3DGeometry* geom,
                            TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  if (!geom) {
    return false;
  }
  switch (geom->GetGeometryType()) {
    case Smt_3DGeo::GT3DPoint: {
      const auto* p = static_cast<const Smt_3DGeo::Smt3DPoint*>(geom);
      tessellate_point_xy(p->GetX(), p->GetY(), p->GetZ(), out);
      break;
    }
    case Smt_3DGeo::GT3DLineString:
    case Smt_3DGeo::GT3DLinearRing: {
      const auto* line = static_cast<const Smt_3DGeo::Smt3DLineString*>(geom);
      const int n = line->GetNumPoints();
      for (int i = 0; i + 1 < n; ++i) {
        tessellate_segment(line->GetX(i), line->GetY(i), line->GetZ(i),
                           line->GetX(i + 1), line->GetY(i + 1),
                           line->GetZ(i + 1), out);
      }
      break;
    }
    case Smt_3DGeo::GT3DSurface: {
      const auto* surf = static_cast<const Smt_3DGeo::Smt3DSurface*>(geom);
      const int np = surf->GetPointCount();
      const int nt = surf->GetTriangleCount();
      for (int i = 0; i < np; ++i) {
        const Smt_3DGeo::Smt3DPoint pt = surf->GetPoint(i);
        append_xyz(out, static_cast<float>(pt.GetX()),
                   static_cast<float>(pt.GetY()),
                   static_cast<float>(pt.GetZ()));
      }
      for (int i = 0; i < nt; ++i) {
        const Smt_Core::Smt3DTriangle tri = surf->GetTriangle(i);
        if (tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= np || tri.b >= np ||
            tri.c >= np) {
          continue;
        }
        out.indices.push_back(static_cast<uint32_t>(tri.a));
        out.indices.push_back(static_cast<uint32_t>(tri.b));
        out.indices.push_back(static_cast<uint32_t>(tri.c));
      }
      break;
    }
    case Smt_3DGeo::GT3DMultiPoint:
    case Smt_3DGeo::GT3DMultiLineString:
    case Smt_3DGeo::GT3DGeometryCollection: {
      const auto* col =
          static_cast<const Smt_3DGeo::Smt3DGeometryCollection*>(geom);
      const int n = col->GetNumGeometries();
      TessMesh part;
      for (int i = 0; i < n; ++i) {
        if (!tessellate_3d_geometry(col->GetGeometryRef(i), part)) {
          continue;
        }
        const uint32_t base = vert_count(out);
        out.positions.insert(out.positions.end(), part.positions.begin(),
                             part.positions.end());
        for (uint32_t idx : part.indices) {
          out.indices.push_back(base + idx);
        }
      }
      break;
    }
    default:
      return false;
  }
  return !out.indices.empty();
}

}  // namespace scene
}  // namespace sdb
