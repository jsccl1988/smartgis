// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/paint/fill.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gis/style/eval/style_rules.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout/geom/attrs.h"
#include "vista/component/map/layout/geom/clip.h"
#include "vista/component/map/layout/geom/geom_walk.h"
#include "vista/component/map/layout/geom/mesh_emit.h"
#include "vista/component/map/layout/paint/source_index.h"
#include "vista/component/map/layout/paint/tess_jobs.h"
#include "vista/component/map/layout/policy/gen.h"
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

bool tile_intersects_view(const LayoutInput& in, double min_x, double min_y,
                          double max_x, double max_y) {
  const double x0 = (std::max)(min_x, in.view.min_x);
  const double x1 = (std::min)(max_x, in.view.max_x);
  const double y0 = (std::max)(min_y, in.view.min_y);
  const double y1 = (std::min)(max_y, in.view.max_y);
  return x1 > x0 && y1 > y0;
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
    // A DEM slot that misses the view must not clip land to empty — that
    // pairs with emit_hillshade dropping the sheet and leaves ocean-only.
    if (!tile_intersects_view(in, hs->min_x, hs->min_y, hs->max_x, hs->max_y)) {
      return false;
    }
    return true;
  }
  if (in.have_dem_clip) {
    if (!tile_intersects_view(in, in.dem_clip.min_x, in.dem_clip.min_y,
                              in.dem_clip.max_x, in.dem_clip.max_y)) {
      return false;
    }
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

// Four distinguishable pastels. Adjacent land polygons must not share one.
constexpr uint32_t kProvinceColor[4] = {
    0xFFE7C98Au,
    0xFF8FBF7Au,
    0xFF7EB0D4u,
    0xFFE09A8Au,
};

struct ProvinceEdge {
  uint64_t a = 0;
  uint64_t b = 0;
  bool operator==(const ProvinceEdge& o) const { return a == o.a && b == o.b; }
};

struct ProvinceEdgeHash {
  size_t operator()(const ProvinceEdge& e) const {
    return static_cast<size_t>(e.a ^ (e.b * 0x9e3779b97f4a7c15ull));
  }
};

uint64_t quantize_province_vertex(double x, double y) {
  const auto qx = static_cast<int32_t>(std::llround(x * 250.0));
  const auto qy = static_cast<int32_t>(std::llround(y * 250.0));
  return (static_cast<uint64_t>(static_cast<uint32_t>(qx)) << 32) |
         static_cast<uint32_t>(qy);
}

void note_ring_edges(
    const OGRLinearRing* ring, size_t job,
    std::unordered_map<ProvinceEdge, std::vector<size_t>, ProvinceEdgeHash>*
        edges) {
  if (!ring || !edges) {
    return;
  }
  const int n = ring->getNumPoints();
  if (n < 2) {
    return;
  }
  uint64_t prev = 0;
  bool have = false;
  for (int i = 0; i < n; ++i) {
    const uint64_t key = quantize_province_vertex(ring->getX(i), ring->getY(i));
    if (!have) {
      prev = key;
      have = true;
      continue;
    }
    if (key == prev) {
      continue;
    }
    ProvinceEdge edge{prev, key};
    if (edge.a > edge.b) {
      std::swap(edge.a, edge.b);
    }
    (*edges)[edge].push_back(job);
    prev = key;
  }
}

void note_geom_edges(
    const OGRGeometry* geom, size_t job,
    std::unordered_map<ProvinceEdge, std::vector<size_t>, ProvinceEdgeHash>*
        edges) {
  if (!geom || !edges) {
    return;
  }
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPolygon) {
    note_ring_edges(geom->toPolygon()->getExteriorRing(), job, edges);
    return;
  }
  if (flat == wkbMultiPolygon || flat == wkbGeometryCollection) {
    const OGRGeometryCollection* col = geom->toGeometryCollection();
    if (!col) {
      return;
    }
    const int n = col->getNumGeometries();
    for (int i = 0; i < n; ++i) {
      note_geom_edges(col->getGeometryRef(i), job, edges);
    }
  }
}

// Map four-color on land polygons that share a quantized border. One feature
// keeps the style color. Greedy coloring uses four pastels; a fifth conflict
// reuses the least-used neighbor color.
void apply_province_colors(
    const std::vector<const gis::style::StyleLayer*>& fill_layers,
    std::vector<FillJob>* jobs) {
  if (!jobs || jobs->size() < 2 || fill_layers.empty()) {
    return;
  }
  std::vector<size_t> land;
  land.reserve(jobs->size());
  for (size_t i = 0; i < jobs->size(); ++i) {
    const size_t ord = (*jobs)[i].layer_ord;
    if (ord >= fill_layers.size() || !fill_layers[ord]) {
      continue;
    }
    if (fill_layers[ord]->source_layer == "land") {
      land.push_back(i);
    }
  }
  if (land.size() < 2) {
    return;
  }

  std::unordered_map<ProvinceEdge, std::vector<size_t>, ProvinceEdgeHash> edges;
  edges.reserve(land.size() * 64);
  for (size_t job : land) {
    note_geom_edges((*jobs)[job].geom, job, &edges);
  }

  std::vector<std::vector<size_t>> adj(jobs->size());
  for (const auto& entry : edges) {
    std::vector<size_t> owners = entry.second;
    std::sort(owners.begin(), owners.end());
    owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
    for (size_t a = 0; a < owners.size(); ++a) {
      for (size_t b = a + 1; b < owners.size(); ++b) {
        adj[owners[a]].push_back(owners[b]);
        adj[owners[b]].push_back(owners[a]);
      }
    }
  }

  std::sort(land.begin(), land.end(), [&](size_t a, size_t b) {
    return adj[a].size() > adj[b].size();
  });

  std::vector<int> color(jobs->size(), -1);
  for (size_t job : land) {
    bool used[4] = {};
    for (size_t nb : adj[job]) {
      const int c = color[nb];
      if (c >= 0 && c < 4) {
        used[c] = true;
      }
    }
    int pick = 0;
    while (pick < 4 && used[pick]) {
      ++pick;
    }
    if (pick == 4) {
      int counts[4] = {};
      for (size_t nb : adj[job]) {
        const int c = color[nb];
        if (c >= 0 && c < 4) {
          ++counts[c];
        }
      }
      pick = 0;
      for (int c = 1; c < 4; ++c) {
        if (counts[c] < counts[pick]) {
          pick = c;
        }
      }
    }
    color[job] = pick;
    (*jobs)[job].paint.fill_color = kProvinceColor[pick];
    (*jobs)[job].paint.fill_opacity = 1.f;
  }
}

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
    // Area is omitted entirely when a raster sheet hits the view (see
    // order_paint_bands). This clip only runs for the area-only band.
    batches.visit(layer, layers, [&](const LayerBatch& batch) {
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        if (layout_gen_stale(in)) {
          return;
        }
        const OGRGeometry* raw = batch.geoms[i];
        const OGRGeometry* geom = nullptr;
        // Cream land stays under the jet sheet. Clip it to the DEM footprint
        // so the fill does not spill past the hillshade as a second body,
        // and so a missing sheet still has a land polygon.
        LayoutTile land_clip;
        const LayoutTile* use_tile = clip_tile;
        LayoutTile hs;
        if (layer.source_layer == "land" && dem_land_clip_tile(in, &hs)) {
          land_clip = hs;
          if (clip_tile) {
            land_clip.min_x = (std::max)(land_clip.min_x, clip_tile->min_x);
            land_clip.min_y = (std::max)(land_clip.min_y, clip_tile->min_y);
            land_clip.max_x = (std::min)(land_clip.max_x, clip_tile->max_x);
            land_clip.max_y = (std::min)(land_clip.max_y, clip_tile->max_y);
          }
          use_tile = &land_clip;
        }
        if (!prepare_tile_clip(raw, use_tile, &clip_store, &geom) || !geom) {
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
  apply_province_colors(fill_layers, &jobs);
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
