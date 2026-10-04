// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/emit.h"

#include "base/trace/event/process_trace.h"
#include "gis/style/style_types.h"
#include "vista/map/layout/fill.h"
#include "vista/map/layout/line.h"
#include "vista/map/layout/point.h"
#include "vista/map/layout/raster.h"
#include "vista/map/layout/slice_key.h"
#include "vista/map/layout/symbol.h"
#include "vista/map/layout/view_metrics.h"

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

template <class EmitFn>
void emit_tiled_layer(const gis::style::StyleLayer& layer, const LayoutInput& in,
                      MapIR* frame, EmitFn&& emit_dirty) {
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
  for (const gis::style::StyleLayer* layer : visible) {
    if (!layer || layout_gen_stale(in)) {
      return;
    }
    const bool symbols_only = in.reuse_world_items;
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
          BASE_TRACE_EVENT("emit_heatmap", "map2d.layout");
          emit_tiled_layer(*layer, in, frame, [&](const LayoutTile* tile) {
            emit_heatmap(*layer, in, layers, wupp, frame, tile);
          });
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
  }
}

}  // namespace detail
}  // namespace vista
