// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/terrain/mesh/tessellate.h"

#include <atomic>
#include <cstdint>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "gis/carto/tile/provider_tile_layer.h"
#include "gis/map/map_layer.h"
#include "vista/world/terrain/mesh/fill_tess.h"
#include "vista/world/terrain/mesh/line_tess.h"
#include "vista/world/terrain/mesh/mesh_append.h"
#include "vista/world/terrain/mesh/mesh_scratch.h"
#include "vista/world/terrain/mesh/tess_trace.h"
#include "ogrsf_frmts.h"

namespace vista {

double line_half_width_world(double pixel_width, double world_units_per_pixel) {
  if (pixel_width <= 0 || world_units_per_pixel <= 0) {
    return 0;
  }
  return 0.5 * pixel_width * world_units_per_pixel;
}

double line_half_width_from_envelope(double pixel_width,
                                     double envelope_world_width,
                                     double viewport_width_px) {
  if (pixel_width <= 0 || envelope_world_width <= 0 || viewport_width_px <= 0) {
    return 0;
  }
  return line_half_width_world(pixel_width,
                               envelope_world_width / viewport_width_px);
}

double resolve_line_half_width(const LineTessOptions& options) {
  if (options.pixel_width > 0 && options.world_units_per_pixel > 0) {
    return line_half_width_world(options.pixel_width,
                                 options.world_units_per_pixel);
  }
  return options.half_width;
}

bool tessellate_geometry(const OGRGeometry* geom, TessMesh& out) {
  return tessellate_geometry(geom, FillTessOptions{}, out);
}

bool tessellate_geometry(const OGRGeometry* geom,
                         const FillTessOptions& fill_options, TessMesh& out) {
  detail::ScopedTessCpu cpu(&detail::tess_trace_stats().geom_us);
  if (base::trace::tracing_enabled()) {
    detail::tess_trace_stats().geom_n.fetch_add(1, std::memory_order_relaxed);
  }
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!detail::tessellate_geom_into(geom, fill_options, out)) {
    return false;
  }
  return !out.indices.empty();
}

bool tessellate_geoms(const OGRGeometry* const* geoms, size_t count,
                      TessMesh& out) {
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!geoms || count == 0) {
    return false;
  }
  const FillTessOptions opts;
  for (size_t i = 0; i < count; ++i) {
    detail::tessellate_geom_into(geoms[i], opts, out);
  }
  return !out.indices.empty();
}

bool tessellate_layer(OGRLayer* layer, TessMesh& out) {
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!layer) {
    return false;
  }
  const FillTessOptions opts;
  layer->ResetReading();
  while (OGRFeature* feat = layer->GetNextFeature()) {
    detail::tessellate_geom_into(feat->GetGeometryRef(), opts, out);
    OGRFeature::DestroyFeature(feat);
  }
  return !out.indices.empty();
}

namespace {

bool tessellate_ogr_tin(const OGRTriangulatedSurface* mesh,
                        TessMesh& out,
                        bool keep_z) {
  detail::reset_mesh(out);
  if (!mesh || mesh->IsEmpty()) {
    return false;
  }
  OGRTriangulatedSurface& writable =
      const_cast<OGRTriangulatedSurface&>(*mesh);
  const int nt = writable.getNumGeometries();
  for (int i = 0; i < nt; ++i) {
    OGRPolygon* patch = writable.getGeometryRef(i);
    if (patch == nullptr) {
      continue;
    }
    OGRLinearRing* ring = patch->getExteriorRing();
    if (ring == nullptr || ring->getNumPoints() < 3) {
      continue;
    }
    OGRPoint a;
    OGRPoint b;
    OGRPoint c;
    ring->getPoint(0, &a);
    ring->getPoint(1, &b);
    ring->getPoint(2, &c);
    const uint32_t base = static_cast<uint32_t>(out.positions.size() / 3);
    const float za = keep_z ? static_cast<float>(a.getZ()) : 0.f;
    const float zb = keep_z ? static_cast<float>(b.getZ()) : 0.f;
    const float zc = keep_z ? static_cast<float>(c.getZ()) : 0.f;
    detail::append_xyz(out, static_cast<float>(a.getX()),
                       static_cast<float>(a.getY()), za);
    detail::append_xyz(out, static_cast<float>(b.getX()),
                       static_cast<float>(b.getY()), zb);
    detail::append_xyz(out, static_cast<float>(c.getX()),
                       static_cast<float>(c.getY()), zc);
    out.indices.push_back(base);
    out.indices.push_back(base + 1);
    out.indices.push_back(base + 2);
  }
  return !out.indices.empty();
}

}  // namespace

bool tessellate_3d_surface(const OGRTriangulatedSurface* surf, TessMesh& out) {
  return tessellate_ogr_tin(surf, out, true);
}

bool tessellate_3d_geometry(const OGRGeometry* geom, TessMesh& out) {
  return tessellate_geometry(geom, out);
}

bool tessellate_arc(const OGRLineString* arc, TessMesh& out) {
  detail::reset_mesh(out);
  if (!arc) {
    return false;
  }
  detail::tessellate_line_legacy(arc, out);
  return !out.indices.empty();
}

bool tessellate_fan(const OGRPolygon* fan, TessMesh& out) {
  detail::reset_mesh(out);
  if (!fan) {
    return false;
  }
  detail::tessellate_polygon(fan, FillTessOptions{}, out);
  return !out.indices.empty();
}

bool tessellate_tin(const OGRTriangulatedSurface* tin, TessMesh& out) {
  return tessellate_ogr_tin(tin, out, false);
}

bool tessellate_grid(const OGRMultiPoint* grid, int nx, int ny, TessMesh& out) {
  detail::reset_mesh(out);
  if (!grid || grid->IsEmpty() || nx < 2 || ny < 2) {
    return false;
  }
  auto node_xy = [&](int i, int j, double* x, double* y) -> bool {
    const int idx = j * nx + i;
    if (idx < 0 || idx >= grid->getNumGeometries() || x == nullptr ||
        y == nullptr) {
      return false;
    }
    const OGRPoint* pt = const_cast<OGRMultiPoint*>(grid)->getGeometryRef(idx);
    if (pt == nullptr) {
      return false;
    }
    *x = pt->getX();
    *y = pt->getY();
    return true;
  };
  for (int j = 0; j + 1 < ny; ++j) {
    for (int i = 0; i + 1 < nx; ++i) {
      double x00 = 0;
      double y00 = 0;
      double x01 = 0;
      double y01 = 0;
      double x11 = 0;
      double y11 = 0;
      double x10 = 0;
      double y10 = 0;
      if (!node_xy(i, j, &x00, &y00) || !node_xy(i + 1, j, &x01, &y01) ||
          !node_xy(i + 1, j + 1, &x11, &y11) ||
          !node_xy(i, j + 1, &x10, &y10)) {
        return false;
      }
      const uint32_t base = detail::vert_count(out);
      detail::append_xyz(out, static_cast<float>(x00), static_cast<float>(y00),
                         0);
      detail::append_xyz(out, static_cast<float>(x01), static_cast<float>(y01),
                         0);
      detail::append_xyz(out, static_cast<float>(x11), static_cast<float>(y11),
                         0);
      detail::append_xyz(out, static_cast<float>(x10), static_cast<float>(y10),
                         0);
      detail::append_quad_indices(out, base);
    }
  }
  return !out.indices.empty();
}

bool tessellate_raster_layer(const gis::MapLayer* slot, TessMesh& out) {
  detail::reset_mesh(out);
  if (!slot) {
    return false;
  }
  gis::Envelope env;
  slot->get_envelope(&env);
  if (!detail::rect_has_area(env.MinX, env.MinY, env.MaxX, env.MaxY)) {
    return false;
  }
  detail::append_quad(env.MinX, env.MinY, env.MaxX, env.MaxY, out);
  // Raster/tile quads are textured when the layer carries image bytes.
  out.has_image = true;
  return !out.indices.empty();
}

bool tessellate_tile_layer(const gis::MapLayer* slot, TessMesh& out) {
  detail::reset_mesh(out);
  if (!slot) {
    return false;
  }
  const gis::tile::ProviderTileLayer* tile = slot->tile();
  if (!tile) {
    return tessellate_raster_layer(slot, out);
  }
  const int n = tile->GetTileCount();
  if (n <= 0) {
    return false;
  }
  bool any_image = false;
  for (int i = 0; i < n; ++i) {
    const gis::tile::TileImage* img = tile->GetTile(i);
    if (!img) {
      continue;
    }
    const gis::Envelope& r = img->world_rect;
    if (!detail::rect_has_area(r.MinX, r.MinY, r.MaxX, r.MaxY)) {
      continue;
    }
    detail::append_quad(r.MinX, r.MinY, r.MaxX, r.MaxY, out);
    if (!img->bytes.empty()) {
      any_image = true;
    }
  }
  out.has_image = any_image;
  return !out.indices.empty();
}

bool tessellate_line(const OGRLineString* line, TessMesh& out) {
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!line || line->getNumPoints() < 2) {
    return false;
  }
  detail::tessellate_line_legacy(line, out);
  return !out.indices.empty();
}

bool tessellate_line(const OGRLineString* line, const LineTessOptions& options,
                     TessMesh& out) {
  detail::ScopedTessCpu cpu(&detail::tess_trace_stats().line_us);
  if (base::trace::tracing_enabled()) {
    detail::tess_trace_stats().line_n.fetch_add(1, std::memory_order_relaxed);
  }
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!line) {
    return false;
  }
  auto pts_holder = detail::poly_pt_vec_pool().allocate();
  std::vector<detail::PolyPt>& pts = *pts_holder;
  {
    detail::ScopedTessCpu points_cpu(&detail::tess_trace_stats().line_points_us);
    detail::line_to_points_decimated(line, options, pts);
  }
  return detail::append_styled_polyline(pts, options, out);
}

bool tessellate_polyline(const float* xyz, size_t point_count,
                         size_t stride_floats, const LineTessOptions& options,
                         TessMesh& out) {
  detail::clear_tessellate_tls_scratch();
  detail::reset_mesh(out);
  if (!xyz || point_count < 2 || (stride_floats != 2 && stride_floats != 3)) {
    return false;
  }
  auto pts_holder = detail::poly_pt_vec_pool().allocate();
  std::vector<detail::PolyPt>& pts = *pts_holder;
  pts.reserve(point_count);
  for (size_t i = 0; i < point_count; ++i) {
    const float* p = xyz + i * stride_floats;
    pts.push_back({p[0], p[1], stride_floats == 3 ? p[2] : 0.f});
  }
  return detail::append_styled_polyline(pts, options, out);
}

bool tessellate_aabb(double min_x, double min_y, double min_z, double max_x,
                     double max_y, double max_z, TessMesh& out) {
  detail::reset_mesh(out);
  if (max_x < min_x || max_y < min_y || max_z < min_z) {
    return false;
  }
  if (max_x - min_x < 1e-9) {
    max_x = min_x + 1e-3;
  }
  if (max_y - min_y < 1e-9) {
    max_y = min_y + 1e-3;
  }
  if (max_z - min_z < 1e-9) {
    max_z = min_z + 1e-3;
  }
  const float x0 = static_cast<float>(min_x);
  const float y0 = static_cast<float>(min_y);
  const float z0 = static_cast<float>(min_z);
  const float x1 = static_cast<float>(max_x);
  const float y1 = static_cast<float>(max_y);
  const float z1 = static_cast<float>(max_z);
  const float corners[8][3] = {
      {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
      {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
  };
  for (int i = 0; i < 8; ++i) {
    out.positions.push_back(corners[i][0]);
    out.positions.push_back(corners[i][1]);
    out.positions.push_back(corners[i][2]);
  }
  const uint32_t faces[12][3] = {
      {0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1},
      {1, 5, 6}, {1, 6, 2}, {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0},
  };
  for (int i = 0; i < 12; ++i) {
    out.indices.push_back(faces[i][0]);
    out.indices.push_back(faces[i][1]);
    out.indices.push_back(faces[i][2]);
  }
  return true;
}

bool tessellate_point_cloud(const float* xyz, size_t point_count,
                            float half_extent, TessMesh& out) {
  detail::reset_mesh(out);
  if (!xyz || point_count == 0) {
    return false;
  }
  float half = half_extent;
  if (half <= 0.f) {
    float min_x = xyz[0];
    float min_y = xyz[1];
    float min_z = xyz[2];
    float max_x = min_x;
    float max_y = min_y;
    float max_z = min_z;
    for (size_t i = 1; i < point_count; ++i) {
      const float x = xyz[i * 3];
      const float y = xyz[i * 3 + 1];
      const float z = xyz[i * 3 + 2];
      if (x < min_x) {
        min_x = x;
      }
      if (y < min_y) {
        min_y = y;
      }
      if (z < min_z) {
        min_z = z;
      }
      if (x > max_x) {
        max_x = x;
      }
      if (y > max_y) {
        max_y = y;
      }
      if (z > max_z) {
        max_z = z;
      }
    }
    const float dx = max_x - min_x;
    const float dy = max_y - min_y;
    const float dz = max_z - min_z;
    const float span = (dx > dy ? dx : dy) > dz ? (dx > dy ? dx : dy) : dz;
    half = span > 0.f ? span * 0.0025f : 0.01f;
    if (half < 1e-4f) {
      half = 1e-4f;
    }
  }
  out.positions.reserve(point_count * 8 * 3);
  out.indices.reserve(point_count * 36);
  for (size_t i = 0; i < point_count; ++i) {
    const float x = xyz[i * 3];
    const float y = xyz[i * 3 + 1];
    const float z = xyz[i * 3 + 2];
    const uint32_t base = static_cast<uint32_t>(out.positions.size() / 3);
    // Axis-aligned cube (was a single XY triangle — edge-on from orbit).
    const float hx = half;
    const float hy = half;
    const float hz = half;
    const float corners[8][3] = {
        {x - hx, y - hy, z - hz}, {x + hx, y - hy, z - hz},
        {x + hx, y + hy, z - hz}, {x - hx, y + hy, z - hz},
        {x - hx, y - hy, z + hz}, {x + hx, y - hy, z + hz},
        {x + hx, y + hy, z + hz}, {x - hx, y + hy, z + hz},
    };
    for (int c = 0; c < 8; ++c) {
      out.positions.push_back(corners[c][0]);
      out.positions.push_back(corners[c][1]);
      out.positions.push_back(corners[c][2]);
    }
    const uint32_t faces[12][3] = {
        {0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1},
        {1, 5, 6}, {1, 6, 2}, {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0},
    };
    for (int f = 0; f < 12; ++f) {
      out.indices.push_back(base + faces[f][0]);
      out.indices.push_back(base + faces[f][1]);
      out.indices.push_back(base + faces[f][2]);
    }
  }
  return true;
}

bool tessellate_point_cloud_indexed(const float* xyz, const uint32_t* indices,
                                    size_t index_count, float half_extent,
                                    TessMesh& out) {
  detail::reset_mesh(out);
  if (!xyz || !indices || index_count == 0) {
    return false;
  }
  std::vector<float> packed;
  packed.reserve(index_count * 3);
  for (size_t i = 0; i < index_count; ++i) {
    const uint32_t idx = indices[i];
    packed.push_back(xyz[idx * 3]);
    packed.push_back(xyz[idx * 3 + 1]);
    packed.push_back(xyz[idx * 3 + 2]);
  }
  return tessellate_point_cloud(packed.data(), index_count, half_extent, out);
}

}  // namespace vista
