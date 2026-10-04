// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/fill.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/style_types.h"
#include "vista/map/layout/attrs.h"
#include "vista/map/layout/emit.h"
#include "vista/map/layout/geom_walk.h"
#include "vista/map/layout/mesh_emit.h"
#include "vista/map/layout/pack.h"
#include "vista/map/layout/tess_grain.h"
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
                          MapFrame* frame) {
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
                         MapFrame* frame, const LayoutTile* clip_tile) {
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
                double wupp, MapFrame* frame, const LayoutTile* clip_tile) {
  if (fill_layers.empty()) {
    return;
  }
  // Index batches by source-layer so each style layer skips unrelated slots
  // (land/water/… walks used to scan every batch).
  std::unordered_map<std::string, std::vector<const LayerBatch*>> by_source;
  by_source.reserve(layers.size() * 2);
  for (const LayerBatch& batch : layers) {
    by_source[batch.source_layer].push_back(&batch);
  }
  auto visit_batches = [&](const gis::style::StyleLayer& layer, auto&& fn) {
    if (layer.source_layer.empty()) {
      for (const LayerBatch& batch : layers) {
        fn(batch);
      }
      return;
    }
    const auto it = by_source.find(layer.source_layer);
    if (it == by_source.end()) {
      return;
    }
    for (const LayerBatch* batch : it->second) {
      fn(*batch);
    }
  };

  std::vector<FillJob> jobs;
  std::vector<std::unique_ptr<OGRGeometry>> clip_store;
  for (size_t li = 0; li < fill_layers.size(); ++li) {
    if (layout_gen_stale(in)) {
      return;
    }
    const gis::style::StyleLayer& layer = *fill_layers[li];
    visit_batches(layer, [&](const LayerBatch& batch) {
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
  auto to_item = [fill_opts](const FillJob& job) -> std::pair<DrawItem, bool> {
    vista::TessMesh mesh;
    if (!vista::tessellate_geometry(job.geom, fill_opts, mesh) ||
        mesh.indices.empty()) {
      return {{}, false};
    }
    DrawItem item = mesh_item(mesh, DrawKind::kFill, job.paint.fill_color,
                              job.paint.fill_opacity);
    if (job.pattern) {
      item.symbol_id = job.paint.fill_pattern;
    }
    return {std::move(item), true};
  };
  if (!vista_layout_parallel_enabled() || jobs.size() < kParallelTessMinGeoms) {
    for (const FillJob& job : jobs) {
      if (layout_gen_stale(in)) {
        return;
      }
      auto [item, ok] = to_item(job);
      if (ok) {
        frame->items.push_back(std::move(item));
      }
    }
    return;
  }
  std::vector<DrawItem> items(jobs.size());
  std::vector<char> valid(jobs.size(), 0);
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, jobs.size(),
      [&](size_t i) {
        if (layout_gen_stale(in)) {
          return;
        }
        auto [item, ok] = to_item(jobs[i]);
        if (!ok) {
          return;
        }
        items[i] = std::move(item);
        valid[i] = 1;
      },
      kParallelTessGrain);
  // Preserve layer order even if jobs were interleaved in the vector by layer.
  for (size_t li = 0; li < fill_layers.size(); ++li) {
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (valid[i] && jobs[i].layer_ord == li) {
        frame->items.push_back(std::move(items[i]));
      }
    }
  }
}

void emit_fill(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapFrame* frame, const LayoutTile* clip_tile) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_fills(one, in, layers, wupp, frame, clip_tile);
}

}  // namespace detail
}  // namespace vista
