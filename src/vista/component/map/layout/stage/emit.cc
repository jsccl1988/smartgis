// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/stage/emit.h"

#include "base/trace/event/process_trace.h"
#include "gis/style/style_types.h"
#include "vista/component/map/layout/paint/fill.h"
#include "vista/component/map/layout/paint/line.h"
#include "vista/component/map/layout/paint/point.h"
#include "vista/component/map/layout/paint/raster.h"
#include "vista/component/map/layout/paint/symbol.h"
#include "vista/component/map/layout/policy/gen.h"
#include "vista/component/map/layout/policy/slice_key.h"
#include "vista/component/map/layout/policy/view_metrics.h"

namespace vista {
namespace detail {
namespace {

void tag_new_items(MapIR* frame, size_t begin, uint64_t key) {
  if (!frame || key == 0) {
    return;
  }
  for (size_t i = begin; i < frame->items.size(); ++i) {
    if (frame->items[i].cache_key == 0) {
      frame->items[i].cache_key = key;
    }
  }
}

bool splice_retained(const LayoutInput& in, uint64_t key, MapIR* frame) {
  if (!in.retained_slices || key == 0) {
    return false;
  }
  const std::vector<DrawItem>* hit = in.retained_slices->find(key);
  if (!hit || hit->empty()) {
    return false;
  }
  frame->items.insert(frame->items.end(), hit->begin(), hit->end());
  return true;
}

std::vector<const gis::style::StyleLayer*> order_paint_bands(
    const LayoutInput& in,
    const std::vector<const gis::style::StyleLayer*>& visible) {
  (void)in;
  // Bands match GPU encode: bake/raster underlay → area → heatmap → lines →
  // points → labels. Style document order inside a band is preserved.
  std::vector<const gis::style::StyleLayer*> background;
  std::vector<const gis::style::StyleLayer*> underlay;
  std::vector<const gis::style::StyleLayer*> areas;
  std::vector<const gis::style::StyleLayer*> heatmap;
  std::vector<const gis::style::StyleLayer*> lines;
  std::vector<const gis::style::StyleLayer*> points;
  std::vector<const gis::style::StyleLayer*> labels;
  std::vector<const gis::style::StyleLayer*> rest;
  background.reserve(visible.size());
  for (const gis::style::StyleLayer* layer : visible) {
    if (!layer) {
      continue;
    }
    switch (layer->type) {
      case gis::style::LayerType::kBackground:
        background.push_back(layer);
        break;
      case gis::style::LayerType::kHillshade:
      case gis::style::LayerType::kRaster:
        underlay.push_back(layer);
        break;
      case gis::style::LayerType::kFill:
      case gis::style::LayerType::kFillExtrusion:
        areas.push_back(layer);
        break;
      case gis::style::LayerType::kHeatmap:
        heatmap.push_back(layer);
        break;
      case gis::style::LayerType::kLine:
        lines.push_back(layer);
        break;
      case gis::style::LayerType::kCircle:
        points.push_back(layer);
        break;
      case gis::style::LayerType::kSymbol:
        labels.push_back(layer);
        break;
      default:
        rest.push_back(layer);
        break;
    }
  }
  std::vector<const gis::style::StyleLayer*> ordered;
  ordered.reserve(visible.size());
  auto append = [&ordered](const std::vector<const gis::style::StyleLayer*>& band) {
    ordered.insert(ordered.end(), band.begin(), band.end());
  };
  append(background);
  append(underlay);
  append(areas);
  append(heatmap);
  append(lines);
  append(points);
  append(labels);
  append(rest);
  return ordered;
}

LayoutTile make_view_clip_tile(const View& view) {
  LayoutTile view_clip;
  view_clip.tx = 0;
  view_clip.ty = 0;
  // Edge skirt plus a pan margin so InteractiveReuse can move the camera
  // without presenting a cleared strip (and without replaying zoom-stale
  // glyph quads to fill that strip).
  const double outset =
      layout_tile_outset_world(view) + layout_pan_coverage_world(view);
  view_clip.min_x = view.min_x - outset;
  view_clip.min_y = view.min_y - outset;
  view_clip.max_x = view.max_x + outset;
  view_clip.max_y = view.max_y + outset;
  return view_clip;
}

template <class EmitFn>
void emit_tiled_layer(const gis::style::StyleLayer& layer, const LayoutInput& in,
                      MapIR* frame, EmitFn&& emit_dirty) {
  // Cold / full rebuild has no retained slices. Multi-tile emit then only
  // multiplies OGR clip × tess (~15 AABB cells at 1280×720) with nothing to
  // splice — Debug china first present paid ~0.8s+ in emit_fill. One view
  // clip keeps tess/GDI on the visible footprint; warm StaticReuse keeps
  // the per-tile splice path.
  if (!in.retained_slices) {
    const LayoutTile view_clip = make_view_clip_tile(in.view);
    emit_dirty(&view_clip);
    return;
  }
  const std::vector<LayoutTile> tiles = enumerate_layout_tiles(in.view);
  if (tiles.empty()) {
    emit_dirty(nullptr);
    return;
  }
  for (const LayoutTile& tile : tiles) {
    if (layout_gen_stale(in)) {
      return;
    }
    const uint64_t key = layout_slice_key(layer.id, in.zoom, tile.tx, tile.ty);
    if (splice_retained(in, key, frame)) {
      continue;
    }
    const size_t begin = frame->items.size();
    emit_dirty(&tile);
    tag_new_items(frame, begin, key);
  }
}

}  // namespace

void emit_visible_layers(const LayoutInput& in,
                         const std::vector<LayerBatch>& layers,
                         const std::vector<const gis::style::StyleLayer*>& visible,
                         LabelGrid* grid, MapIR* frame) {
  if (!frame || layout_gen_stale(in)) {
    return;
  }
  const double wupp = world_units_per_pixel(in.view);
  const float fblc = device_px_per_world(in.view);
  // Bands: background, bake/raster, area, heatmap, lines, points, labels.
  // Cold full rebuilds: consecutive fill/line layers share one parallel_for.
  // Warm retained-slice path stays per-layer so cache_key tagging remains correct.
  const std::vector<const gis::style::StyleLayer*> ordered =
      order_paint_bands(in, visible);
  const bool cold_cross_layer = !in.retained_slices;
  for (size_t vi = 0; vi < ordered.size();) {
    const gis::style::StyleLayer* layer = ordered[vi];
    if (!layer || layout_gen_stale(in)) {
      return;
    }
    const bool symbols_only = in.reuse_world_items;
    if (!symbols_only && cold_cross_layer &&
        layer->type == gis::style::LayerType::kFill) {
      std::vector<const gis::style::StyleLayer*> fills;
      while (vi < ordered.size() && ordered[vi] &&
             ordered[vi]->type == gis::style::LayerType::kFill) {
        fills.push_back(ordered[vi]);
        ++vi;
      }
      BASE_TRACE_EVENT("emit_fill", "map2d.layout");
      const LayoutTile view_clip = make_view_clip_tile(in.view);
      emit_fills(fills, in, layers, wupp, frame, &view_clip);
      continue;
    }
    if (!symbols_only && cold_cross_layer &&
        layer->type == gis::style::LayerType::kLine) {
      std::vector<const gis::style::StyleLayer*> lines;
      while (vi < ordered.size() && ordered[vi] &&
             ordered[vi]->type == gis::style::LayerType::kLine) {
        lines.push_back(ordered[vi]);
        ++vi;
      }
      BASE_TRACE_EVENT("emit_line", "map2d.layout");
      const LayoutTile view_clip = make_view_clip_tile(in.view);
      // Full-view cold emit: AABB cull only — GEOS Intersection of long
      // china rivers/admin edges serializes emit_line wall before tess.
      emit_lines(lines, in, layers, wupp, frame, &view_clip,
                 /*intersect_clip=*/false);
      continue;
    }
    switch (layer->type) {
      case gis::style::LayerType::kBackground:
        if (!symbols_only) {
          apply_background(*layer, in.zoom, frame);
        }
        break;
      case gis::style::LayerType::kRaster:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_raster", "map2d.layout");
          emit_raster(*layer, in, frame);
        }
        break;
      case gis::style::LayerType::kHillshade:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_hillshade", "map2d.layout");
          emit_hillshade(*layer, in, frame);
        }
        break;
      case gis::style::LayerType::kFillExtrusion:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_fill_extrusion", "map2d.layout");
          emit_tiled_layer(*layer, in, frame, [&](const LayoutTile* tile) {
            emit_fill_extrusion(*layer, in, layers, wupp, frame, tile);
          });
        }
        break;
      case gis::style::LayerType::kHeatmap:
        if (!symbols_only) {
          // Density underlay is view-sized (not AABB-sliced). One emit keeps
          // gaussian accumulation correct; splat fallback still clips to view.
          BASE_TRACE_EVENT("emit_heatmap", "map2d.layout");
          const LayoutTile view_clip = make_view_clip_tile(in.view);
          emit_heatmap(*layer, in, layers, wupp, frame, &view_clip);
        }
        break;
      case gis::style::LayerType::kFill:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_fill", "map2d.layout");
          emit_tiled_layer(*layer, in, frame, [&](const LayoutTile* tile) {
            const std::vector<const gis::style::StyleLayer*> one{layer};
            emit_fills(one, in, layers, wupp, frame, tile);
          });
        }
        break;
      case gis::style::LayerType::kLine:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_line", "map2d.layout");
          emit_tiled_layer(*layer, in, frame, [&](const LayoutTile* tile) {
            const std::vector<const gis::style::StyleLayer*> one{layer};
            emit_lines(one, in, layers, wupp, frame, tile);
          });
        }
        break;
      case gis::style::LayerType::kCircle:
        if (!symbols_only) {
          BASE_TRACE_EVENT("emit_circle", "map2d.layout");
          emit_tiled_layer(*layer, in, frame, [&](const LayoutTile* tile) {
            emit_circles(*layer, in, layers, wupp, frame, tile);
          });
        }
        break;
      case gis::style::LayerType::kSymbol:
        {
          BASE_TRACE_EVENT("emit_symbol", "map2d.layout");
          emit_symbols(*layer, in, layers, fblc, grid, frame);
        }
        break;
      default:
        break;
    }
    ++vi;
  }
}

}  // namespace detail
}  // namespace vista
