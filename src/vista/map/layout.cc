// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout orchestrator: collect_visible → pack_geoms → emit_* → coalesce.
// Public signature stays Layout::build.

#include "vista/map/layout.h"

#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "vista/map/collision.h"
#include "vista/map/layout/coalesce.h"
#include "vista/map/layout/collect.h"
#include "vista/map/layout/emit.h"
#include "vista/map/layout/pack.h"
#include "vista/map/layout/view_metrics.h"
#include "vista/mesh/tessellate.h"

namespace vista {

MapIR Layout::build(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers) const {
  // Reset TLS hybrid scratch for this build so label/token temps reuse
  // arena blocks instead of churning the process heap across frames.
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }
  if (base::trace::tracing_enabled()) {
    vista::reset_tess_trace_stats();
  }
  MapIR frame;
  if (!in.style) {
    return frame;
  }
  const float fblc = detail::device_px_per_world(in.view);
  // Constructed here so reset() (inline) uses this TU's LabelGrid layout.
  detail::LabelGrid grid;
  if (detail::screen_ready(in.view)) {
    grid.reset(fblc, static_cast<int>(in.view.width_px),
               static_cast<int>(in.view.height_px));
  }
  const std::vector<const gis::style::StyleLayer*> visible =
      detail::collect_visible(in.style->layers, in.zoom);
  const detail::PackedGeoms packed = detail::pack_geoms(in, layers);
  detail::emit_visible_layers(in, packed.batches, visible, &grid, &frame);
  if (!detail::layout_gen_stale(in)) {
    detail::coalesce_draw_items(&frame.items);
  }
  if (base::trace::tracing_enabled()) {
    vista::flush_tess_trace_stats();
  }
  return frame;
}

}  // namespace vista
