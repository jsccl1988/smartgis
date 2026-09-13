// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/tessellate.h"

#include "algorithm/geo/geometry.h"
#include "sdb/layer/layer.h"
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

void tessellate_line(const OGRLineString* line, TessMesh& out) {
  if (!line) {
    return;
  }
  const int n = line->getNumPoints();
  for (int i = 0; i + 1 < n; ++i) {
    tessellate_segment(line->getX(i), line->getY(i), line->getZ(i),
                       line->getX(i + 1), line->getY(i + 1), line->getZ(i + 1),
                       out);
  }
}

void tessellate_ring_fan(const OGRLinearRing* ring, TessMesh& out) {
  if (!ring) {
    return;
  }
  int n = ring->getNumPoints();
  if (n >= 2 && ring->getX(0) == ring->getX(n - 1) &&
      ring->getY(0) == ring->getY(n - 1)) {
    --n;
  }
  if (n < 3) {
    return;
  }
  const uint32_t base = vert_count(out);
  for (int i = 0; i < n; ++i) {
    append_xyz(out, static_cast<float>(ring->getX(i)),
               static_cast<float>(ring->getY(i)), 0);
  }
  for (int i = 1; i + 1 < n; ++i) {
    out.indices.push_back(base);
    out.indices.push_back(base + static_cast<uint32_t>(i));
    out.indices.push_back(base + static_cast<uint32_t>(i + 1));
  }
}

void tessellate_polygon(const OGRPolygon* poly, TessMesh& out) {
  if (!poly) {
    return;
  }
  tessellate_ring_fan(poly->getExteriorRing(), out);
}

bool tessellate_geom_into(const OGRGeometry* geom, TessMesh& out) {
  if (!geom) {
    return false;
  }
  switch (wkbFlatten(geom->getGeometryType())) {
    case wkbPoint: {
      const auto* p = geom->toPoint();
      tessellate_point_xy(p->getX(), p->getY(), p->getZ(), out);
      return true;
    }
    case wkbLineString:
    case wkbLinearRing:
      tessellate_line(geom->toLineString(), out);
      return true;
    case wkbPolygon:
    case wkbTriangle:
      tessellate_polygon(geom->toPolygon(), out);
      return true;
    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbGeometryCollection:
    case wkbTIN: {
      const auto* col = geom->toGeometryCollection();
      if (!col) {
        return false;
      }
      const int n = col->getNumGeometries();
      bool any = false;
      for (int i = 0; i < n; ++i) {
        if (tessellate_geom_into(col->getGeometryRef(i), out)) {
          any = true;
        }
      }
      return any;
    }
    default:
      return false;
  }
}

void reset_mesh(TessMesh& out) {
  out.positions.clear();
  out.indices.clear();
  out.has_image = false;
}

void append_quad(double min_x, double min_y, double max_x, double max_y,
                 TessMesh& out) {
  const uint32_t base = vert_count(out);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(min_y), 0);
  append_xyz(out, static_cast<float>(max_x), static_cast<float>(max_y), 0);
  append_xyz(out, static_cast<float>(min_x), static_cast<float>(max_y), 0);
  out.indices.push_back(base);
  out.indices.push_back(base + 1);
  out.indices.push_back(base + 2);
  out.indices.push_back(base);
  out.indices.push_back(base + 2);
  out.indices.push_back(base + 3);
}

bool rect_has_area(double min_x, double min_y, double max_x, double max_y) {
  return max_x > min_x && max_y > min_y;
}

}  // namespace

bool tessellate_geometry(const OGRGeometry* geom, TessMesh& out) {
  reset_mesh(out);
  if (!tessellate_geom_into(geom, out)) {
    return false;
  }
  return !out.indices.empty();
}

bool tessellate_geoms(const OGRGeometry* const* geoms, size_t count,
                      TessMesh& out) {
  reset_mesh(out);
  if (!geoms || count == 0) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    tessellate_geom_into(geoms[i], out);
  }
  return !out.indices.empty();
}

bool tessellate_layer(OGRLayer* layer, TessMesh& out) {
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    tessellate_geom_into(feat->GetGeometryRef(), out);
    OGRFeature::DestroyFeature(feat);
  }
  return !out.indices.empty();
}

bool tessellate_3d_surface(const geo::Tin* surf, TessMesh& out) {
  reset_mesh(out);
  if (!surf) {
    return false;
  }
  const int np = surf->get_point_count();
  const int nt = surf->get_triangle_count();
  for (int i = 0; i < np; ++i) {
    const OGRPoint pt = surf->get_point(i);
    append_xyz(out, static_cast<float>(pt.getX()),
               static_cast<float>(pt.getY()), static_cast<float>(pt.getZ()));
  }
  for (int i = 0; i < nt; ++i) {
    const base::Smt3DTriangle tri = surf->get_triangle(i);
    if (tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= np || tri.b >= np ||
        tri.c >= np) {
      continue;
    }
    out.indices.push_back(static_cast<uint32_t>(tri.a));
    out.indices.push_back(static_cast<uint32_t>(tri.b));
    out.indices.push_back(static_cast<uint32_t>(tri.c));
  }
  return !out.indices.empty();
}

bool tessellate_3d_geometry(const OGRGeometry* geom, TessMesh& out) {
  return tessellate_geometry(geom, out);
}

bool tessellate_arc(const OGRLineString* arc, TessMesh& out) {
  reset_mesh(out);
  if (!arc) {
    return false;
  }
  tessellate_line(arc, out);
  return !out.indices.empty();
}

bool tessellate_fan(const OGRPolygon* fan, TessMesh& out) {
  reset_mesh(out);
  if (!fan) {
    return false;
  }
  tessellate_polygon(fan, out);
  return !out.indices.empty();
}

bool tessellate_tin(const geo::Tin* tin, TessMesh& out) {
  reset_mesh(out);
  if (!tin || tin->is_empty()) {
    return false;
  }
  const int np = tin->get_point_count();
  const int nt = tin->get_triangle_count();
  for (int i = 0; i < np; ++i) {
    const OGRPoint pt = tin->get_point(i);
    append_xyz(out, static_cast<float>(pt.getX()),
               static_cast<float>(pt.getY()), 0);
  }
  for (int i = 0; i < nt; ++i) {
    const base::SmtTriangle tri = tin->get_triangle(i);
    if (tri.bDelete || tri.a < 0 || tri.b < 0 || tri.c < 0 || tri.a >= np ||
        tri.b >= np || tri.c >= np) {
      continue;
    }
    out.indices.push_back(static_cast<uint32_t>(tri.a));
    out.indices.push_back(static_cast<uint32_t>(tri.b));
    out.indices.push_back(static_cast<uint32_t>(tri.c));
  }
  return !out.indices.empty();
}

bool tessellate_grid(const geo::Grid* grid, TessMesh& out) {
  reset_mesh(out);
  if (!grid || grid->is_empty()) {
    return false;
  }
  int rows = 0;
  int cols = 0;
  grid->get_size(rows, cols);
  if (rows < 2 || cols < 2) {
    return false;
  }
  for (int i = 0; i + 1 < rows; ++i) {
    for (int j = 0; j + 1 < cols; ++j) {
      const geo::RawPoint p00 = grid->node(i, j);
      const geo::RawPoint p01 = grid->node(i, j + 1);
      const geo::RawPoint p11 = grid->node(i + 1, j + 1);
      const geo::RawPoint p10 = grid->node(i + 1, j);
      const uint32_t base = vert_count(out);
      append_xyz(out, static_cast<float>(p00.x), static_cast<float>(p00.y), 0);
      append_xyz(out, static_cast<float>(p01.x), static_cast<float>(p01.y), 0);
      append_xyz(out, static_cast<float>(p11.x), static_cast<float>(p11.y), 0);
      append_xyz(out, static_cast<float>(p10.x), static_cast<float>(p10.y), 0);
      out.indices.push_back(base);
      out.indices.push_back(base + 1);
      out.indices.push_back(base + 2);
      out.indices.push_back(base);
      out.indices.push_back(base + 2);
      out.indices.push_back(base + 3);
    }
  }
  return !out.indices.empty();
}

bool tessellate_raster_layer(const sdb::SmtRasterLayer* layer,
                             TessMesh& out) {
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  base::fRect rect;
  if (layer->GetRasterRect(rect) != SMT_ERR_NONE ||
      !rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
    base::Envelope env;
    layer->get_envelope(env);
    rect.lb.x = static_cast<float>(env.MinX);
    rect.lb.y = static_cast<float>(env.MinY);
    rect.rt.x = static_cast<float>(env.MaxX);
    rect.rt.y = static_cast<float>(env.MaxY);
  }
  if (!rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
    return false;
  }
  append_quad(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y, out);
  char* buf = nullptr;
  long size = 0;
  long code = 0;
  base::fRect loc;
  if (layer->GetRasterNoClone(buf, size, loc, code) == SMT_ERR_NONE && buf &&
      size > 0) {
    out.has_image = true;
  }
  return !out.indices.empty();
}

bool tessellate_tile_layer(const sdb::SmtTileLayer* layer, TessMesh& out) {
  reset_mesh(out);
  if (!layer) {
    return false;
  }
  const int n = layer->GetTileCount();
  for (int i = 0; i < n; ++i) {
    const base::SmtTile* tile = layer->GetTile(i);
    if (!tile) {
      continue;
    }
    const base::fRect& rect = tile->rtTileRect;
    if (!rect_has_area(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y)) {
      continue;
    }
    append_quad(rect.lb.x, rect.lb.y, rect.rt.x, rect.rt.y, out);
    if (tile->pTileBuf && tile->lTileBufSize > 0) {
      out.has_image = true;
    }
  }
  if (!out.indices.empty()) {
    return true;
  }
  base::Envelope env;
  layer->get_envelope(env);
  if (!rect_has_area(env.MinX, env.MinY, env.MaxX, env.MaxY)) {
    return false;
  }
  append_quad(env.MinX, env.MinY, env.MaxX, env.MaxY, out);
  return !out.indices.empty();
}

}  // namespace scene
}  // namespace sdb
