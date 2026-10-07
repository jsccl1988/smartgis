// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/fill.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gis/style/eval/style_rules.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout/attrs.h"
#include "vista/component/map/layout/clip.h"
#include "vista/component/map/layout/geom_walk.h"
#include "vista/component/map/layout/mesh_emit.h"
#include "vista/component/map/layout/source_index.h"
#include "vista/component/map/layout/tess_jobs.h"
#include "vista/component/map/layout/gen.h"
#include "vista/mesh/tessellate.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

uint32_t darken_argb(uint32_t argb, float factor) {
  const float f = (std::max)(0.f, (std::min)(1.f, factor));
  const uint32_t a = (argb >> 24) & 0xffu;
  const uint32_t r =
      static_cast<uint32_t>(static_cast<float>((argb >> 16) & 0xffu) * f);
  const uint32_t g =
      static_cast<uint32_t>(static_cast<float>((argb >> 8) & 0xffu) * f);
  const uint32_t b =
      static_cast<uint32_t>(static_cast<float>(argb & 0xffu) * f);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

// Jet hillshade is a full land surface (kOver). Cream land under it shows
// through transparent ocean cells and, when Y-mirrored relative to the DEM
// sheet, reads as an upside-down white China north of the jet mass.
// have_dem_clip alone also suppresses cream: hosts may publish the DEM AABB
// a frame before the raster DrawItem is attached (map2d_test dem_clip case).
bool jet_hillshade_active(const LayoutInput& in) {
  if (!in.style) {
    return false;
  }
  if (in.hillshade_tiles.empty() && !in.have_dem_clip) {
    return false;
  }
  for (const gis::style::StyleLayer& layer : in.style->layers) {
    if (layer.type != gis::style::LayerType::kHillshade) {
      continue;
    }
    const auto ramp = layer.paint.find("hillshade-color-ramp");
    if (ramp != layer.paint.end() && ramp->second == "jet") {
      return true;
    }
  }
  return false;
}

bool dem_land_clip_tile(const LayoutInput& in, LayoutTile* hs) {
  if (!hs) {
    return false;
  }
  if (!in.hillshade_tiles.empty()) {
    hs->min_x = in.hillshade_tiles[0].min_x;
    hs->min_y = in.hillshade_tiles[0].min_y;
    hs->max_x = in.hillshade_tiles[0].max_x;
    hs->max_y = in.hillshade_tiles[0].max_y;
    for (const TileSlot& slot : in.hillshade_tiles) {
      hs->min_x = (std::min)(hs->min_x, slot.min_x);
      hs->min_y = (std::min)(hs->min_y, slot.min_y);
      hs->max_x = (std::max)(hs->max_x, slot.max_x);
      hs->max_y = (std::max)(hs->max_y, slot.max_y);
    }
    return true;
  }
  if (in.have_dem_clip) {
    hs->min_x = in.dem_clip.min_x;
    hs->min_y = in.dem_clip.min_y;
    hs->max_x = in.dem_clip.max_x;
    hs->max_y = in.dem_clip.max_y;
    return true;
  }
  return false;
}

void lift_mesh_z(vista::TessMesh* mesh, float z) {
  if (!mesh) {
    return;
  }
  for (size_t i = 2; i < mesh->positions.size(); i += 3) {
    mesh->positions[i] = z;
  }
}

void offset_mesh_xy(vista::TessMesh* mesh, float dx, float dy) {
  if (!mesh) {
    return;
  }
  for (size_t i = 0; i + 2 < mesh->positions.size(); i += 3) {
    mesh->positions[i] += dx;
    mesh->positions[i + 1] += dy;
  }
}

void append_wall_quad(DrawItem* item, float x0, float y0, float x1, float y1,
                      float z0, float z1, float ox, float oy) {
  if (!item) {
    return;
  }
  const uint32_t base = static_cast<uint32_t>(item->vertices.size());
  // Bottom edge at footprint; top edge shifted for 2.5D ortho readability.
  item->vertices.push_back(Vertex{x0, y0, z0, 0, 0});
  item->vertices.push_back(Vertex{x1, y1, z0, 1, 0});
  item->vertices.push_back(Vertex{x1 + ox, y1 + oy, z1, 1, 1});
  item->vertices.push_back(Vertex{x0 + ox, y0 + oy, z1, 0, 1});
  item->indices.push_back(base);
  item->indices.push_back(base + 1);
  item->indices.push_back(base + 2);
  item->indices.push_back(base);
  item->indices.push_back(base + 2);
  item->indices.push_back(base + 3);
}

void emit_extrusion_walls(const OGRGeometry* geom, float z_base, float z_top,
                          float ox, float oy, uint32_t rgba, float opacity,
                          MapIR* frame) {
  if (!geom || !frame) {
    return;
  }
  const OGRwkbGeometryType t =
      wkbFlatten(static_cast<OGRwkbGeometryType>(geom->getGeometryType()));
  if (t == wkbMultiPolygon) {
    const auto* mp = dynamic_cast<const OGRMultiPolygon*>(geom);
    if (!mp) {
      return;
    }
    for (int i = 0; i < mp->getNumGeometries(); ++i) {
      emit_extrusion_walls(mp->getGeometryRef(i), z_base, z_top, ox, oy, rgba,
                           opacity, frame);
    }
    return;
  }
  if (t != wkbPolygon) {
    return;
  }
  const auto* poly = dynamic_cast<const OGRPolygon*>(geom);
  if (!poly || poly->getExteriorRing() == nullptr) {
    return;
  }
  const OGRLinearRing* ring = poly->getExteriorRing();
  const int n = ring->getNumPoints();
  if (n < 4) {
    return;
  }
  DrawItem walls;
  walls.kind = DrawKind::kFill;
  walls.rgba = rgba;
  walls.opacity = opacity;
  walls.pixel_space = false;
  for (int i = 0; i + 1 < n; ++i) {
    append_wall_quad(&walls, static_cast<float>(ring->getX(i)),
                     static_cast<float>(ring->getY(i)),
                     static_cast<float>(ring->getX(i + 1)),
                     static_cast<float>(ring->getY(i + 1)), z_base, z_top, ox,
                     oy);
  }
  if (!walls.indices.empty()) {
    frame->items.push_back(std::move(walls));
  }
}

struct FillJob {
  size_t layer_ord = 0;
  const OGRGeometry* geom = nullptr;
  gis::style::ResolvedPaint paint;
  bool pattern = false;
};

}  // namespace

// Own prism emit: walls then roof. Height/base are world-CRS z; a small xy
// isometric offset makes sides readable on the ortho GDI path.
void emit_fill_extrusion(const gis::style::StyleLayer& layer,
                         const LayoutInput& in,
                         const std::vector<LayerBatch>& layers, double wupp,
                         MapIR* frame, const LayoutTile* clip_tile) {
  if (!frame) {
    return;
  }
  FillTessOptions fill_opts;
  fill_opts.world_units_per_pixel = wupp;
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (const LayerBatch& batch : layers) {
    if (!layer_uses_batch(layer, batch)) {
      continue;
    }
    for (size_t i = 0; i < batch.geoms.size(); ++i) {
      if (layout_gen_stale(in)) {
        return;
      }
      const OGRGeometry* raw = batch.geoms[i];
      const OGRGeometry* geom = nullptr;
      if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
        continue;
      }
        const gis::style::AttrMap& attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      const float z_base = paint.fill_extrusion_base;
      float z_top = paint.fill_extrusion_height;
      if (z_top < z_base) {
        z_top = z_base;
      }
      const float rise = z_top - z_base;
      if (rise <= 0.f) {
        continue;
      }
      // Southeast isometric nudge in world units (readable on cream basemap).
      const float ox = rise * 0.35f;
      const float oy = -rise * 0.25f;
      const uint32_t wall_rgba = darken_argb(paint.fill_extrusion_color, 0.65f);
      emit_extrusion_walls(geom, z_base, z_top, ox, oy, wall_rgba,
                           paint.fill_extrusion_opacity, frame);

      vista::TessMesh roof;
      if (!vista::tessellate_geometry(geom, fill_opts, roof) ||
          roof.indices.empty()) {
        continue;
      }
      lift_mesh_z(&roof, z_top);
      offset_mesh_xy(&roof, ox, oy);
      frame->items.push_back(mesh_item(roof, DrawKind::kFill,
                                       paint.fill_extrusion_color,
                                       paint.fill_extrusion_opacity));
    }
  }
}

// Emit one or more consecutive fill layers. Jobs from all layers share one
// parallel_for; results are appended in layer order (painter z).
void emit_fills(const std::vector<const gis::style::StyleLayer*>& fill_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame, const LayoutTile* clip_tile) {
  if (fill_layers.empty() || !frame) {
    return;
  }
  // Index batches by source-layer so each style layer skips unrelated slots
  // (land/water/… walks used to scan every batch).
  const SourceBatchIndex batches(layers);

  std::vector<FillJob> jobs;
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (size_t li = 0; li < fill_layers.size(); ++li) {
    if (layout_gen_stale(in)) {
      return;
    }
    const gis::style::StyleLayer& layer = *fill_layers[li];
    // Jet sheet replaces cream land; keeping land only feeds the upside-down
    // white ghost through transparent ocean texels.
    if (layer.source_layer == "land" && jet_hillshade_active(in)) {
      continue;
    }
    batches.visit(layer, layers, [&](const LayerBatch& batch) {
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        if (layout_gen_stale(in)) {
          return;
        }
        const OGRGeometry* raw = batch.geoms[i];
        const OGRGeometry* geom = nullptr;
        if (!prepare_tile_clip(raw, clip_tile, &clip_store, &geom) || !geom) {
          continue;
        }
        // Hillshade / dem_clip is the DEM footprint. Land polygons that
        // extend past it (Korea / Mongolia cream slab) stay unshaded —
        // clip land so ocean background shows instead of a hole.
        if (layer.source_layer == "land") {
          LayoutTile hs;
          if (dem_land_clip_tile(in, &hs)) {
            const OGRGeometry* clipped = nullptr;
            if (!prepare_tile_clip(geom, &hs, &clip_store, &clipped) ||
                !clipped) {
              continue;
            }
            geom = clipped;
          }
        }
        const gis::style::AttrMap& attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        gis::style::ResolvedPaint paint;
        gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
        jobs.push_back(FillJob{li, geom, paint,
                               find_symbol(in, paint.fill_pattern) != nullptr});
      }
    });
  }
  if (jobs.empty()) {
    return;
  }
  FillTessOptions fill_opts;
  fill_opts.world_units_per_pixel = wupp;
  auto to_item = [fill_opts](const FillJob& job) -> std::optional<DrawItem> {
    vista::TessMesh mesh;
    if (!vista::tessellate_geometry(job.geom, fill_opts, mesh) ||
        mesh.indices.empty()) {
      return std::nullopt;
    }
    DrawItem item = mesh_item(mesh, DrawKind::kFill, job.paint.fill_color,
                              job.paint.fill_opacity);
    if (job.pattern) {
      item.symbol_id = job.paint.fill_pattern;
    }
    return item;
  };
  run_ordered_tess(in, fill_layers.size(), jobs, to_item, &frame->items);
}

void emit_fill(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapIR* frame, const LayoutTile* clip_tile) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_fills(one, in, layers, wupp, frame, clip_tile);
}

}  // namespace detail
}  // namespace vista
