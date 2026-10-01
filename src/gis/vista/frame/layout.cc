// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout orchestrator: style-order dispatch into fill/line/raster/point/symbol
// policies under detail/layout/.

#include "gis/vista/frame/frame.h"

#include <vector>

#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "gis/present/style/style_rules.h"
#include "gis/vista/frame/detail/collision/collision.h"
#include "gis/vista/frame/detail/layout/fill.h"
#include "gis/vista/frame/detail/layout/line.h"
#include "gis/vista/frame/detail/layout/point.h"
#include "gis/vista/frame/detail/layout/raster.h"
#include "gis/vista/frame/detail/layout/symbol.h"
#include "gis/vista/frame/detail/layout/view_metrics.h"
#include "gis/vista/world/terrain/mesh/tessellate.h"

namespace gis {
namespace vista {

MapFrame Layout::build(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers) const {
  // Reset TLS hybrid scratch for this build so label/token temps reuse
  // arena blocks instead of churning the process heap across frames.
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }
  if (base::trace::tracing_enabled()) {
    gis::reset_tess_trace_stats();
  }
  MapFrame frame;
  if (!in.style) {
    return frame;
  }
  const double wupp = detail::world_units_per_pixel(in.view);
  const float fblc = detail::device_px_per_world(in.view);
  // Constructed here so reset() (inline) uses this TU's LabelGrid layout.
  detail::LabelGrid grid;
  if (detail::screen_ready(in.view)) {
    grid.reset(fblc, static_cast<int>(in.view.width_px),
               static_cast<int>(in.view.height_px));
  }

  std::vector<const gis::style::StyleLayer*> pending_fills;
  std::vector<const gis::style::StyleLayer*> pending_lines;
  auto flush_fills = [&]() {
    if (pending_fills.empty()) {
      return;
    }
    BASE_TRACE_EVENT("emit_fill", "map2d.layout");
    detail::emit_fills(pending_fills, in, layers, wupp, &frame);
    pending_fills.clear();
  };
  auto flush_lines = [&]() {
    if (pending_lines.empty()) {
      return;
    }
    BASE_TRACE_EVENT("emit_line", "map2d.layout");
    detail::emit_lines(pending_lines, in, layers, wupp, &frame);
    pending_lines.clear();
  };

  for (const gis::style::StyleLayer& layer : in.style->layers) {
    if (!gis::style::layer_matches_zoom(layer, in.zoom)) {
      continue;
    }
    switch (layer.type) {
      case gis::style::LayerType::kBackground:
        flush_fills();
        flush_lines();
        detail::apply_background(layer, in.zoom, &frame);
        break;
      case gis::style::LayerType::kRaster:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_raster", "map2d.layout");
          detail::emit_raster(layer, in, &frame);
        }
        break;
      case gis::style::LayerType::kHillshade:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_hillshade", "map2d.layout");
          detail::emit_hillshade(layer, in, &frame);
        }
        break;
      case gis::style::LayerType::kFillExtrusion:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_fill_extrusion", "map2d.layout");
          detail::emit_fill_extrusion(layer, in, layers, wupp, &frame);
        }
        break;
      case gis::style::LayerType::kHeatmap:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_heatmap", "map2d.layout");
          detail::emit_heatmap(layer, in, layers, wupp, &frame);
        }
        break;
      case gis::style::LayerType::kFill:
        flush_lines();
        pending_fills.push_back(&layer);
        break;
      case gis::style::LayerType::kLine:
        flush_fills();
        pending_lines.push_back(&layer);
        break;
      case gis::style::LayerType::kCircle:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_circle", "map2d.layout");
          detail::emit_circles(layer, in, layers, wupp, &frame);
        }
        break;
      case gis::style::LayerType::kSymbol:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_symbol", "map2d.layout");
          detail::emit_symbols(layer, in, layers, fblc, &grid, &frame);
        }
        break;
      default:
        flush_fills();
        flush_lines();
        break;
    }
  }
  flush_fills();
  flush_lines();
  if (base::trace::tracing_enabled()) {
    gis::flush_tess_trace_stats();
  }
  return frame;
}

}  // namespace vista
}  // namespace gis
