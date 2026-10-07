// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/shade/attach.h"

#include "vista/component/map/carto/resolve.h"
#include "vista/component/map/shade/bake.h"

#include <chrono>
#include <utility>

#include "base/trace/event/process_trace.h"
#include "vista/terrain/dem/raster/dem_raster.h"

namespace vista {

HillshadeAttachResult attach_hillshade_slot(LayoutInput* layout,
                                             const HillshadeAttachPolicy& policy) {
  HillshadeAttachResult result;
  if (!layout) {
    return result;
  }
  result.zoom = layout->zoom;
  if (policy.skip) {
    result.kind = HillshadeAttachKind::kSkipped;
    return result;
  }
  if (policy.ready && policy.ready_slot.texture_key != 0) {
    layout->hillshade_tiles.push_back(policy.ready_slot);
    layout->have_dem_clip = true;
    layout->dem_clip = policy.ready_slot;
    result.kind = HillshadeAttachKind::kReused;
    result.slot = policy.ready_slot;
    result.tile_count = layout->hillshade_tiles.size();
    if (!layout->hillshade_tiles.empty()) {
      result.opacity = layout->hillshade_tiles.back().opacity;
      result.texture_key = layout->hillshade_tiles.back().texture_key;
    }
    return result;
  }
  if (!layout->hillshade_tiles.empty()) {
    result.kind = HillshadeAttachKind::kReused;
    return result;
  }

  const gis::style::StyleLayer* hillshade =
      find_hillshade_layer(layout->style, layout->zoom);
  if (!hillshade) {
    result.kind = HillshadeAttachKind::kNoLayer;
    return result;
  }

  result.dem_path = find_sample_dem_path();
  if (result.dem_path.empty()) {
    result.kind = HillshadeAttachKind::kDemMissing;
    return result;
  }

  const auto t0 = std::chrono::steady_clock::now();
  HillshadeBake baked;
  {
    BASE_TRACE_EVENT("HillshadeBake", "startup");
    baked = bake_hillshade_slot(result.dem_path, layout->zoom, *hillshade,
                                 policy.texture_key);
  }
  result.bake_ok = baked.ok;
  result.width = baked.width;
  result.height = baked.height;
  result.rgba_bytes = baked.rgba.size();
  if (baked.ok && baked.width > 0 && baked.height > 0 && !baked.rgba.empty()) {
    layout->have_dem_clip = true;
    layout->dem_clip = baked.slot;
    layout->hillshade_tiles.push_back(baked.slot);
    result.kind = HillshadeAttachKind::kBaked;
    result.slot = baked.slot;
    result.rgba = std::move(baked.rgba);
    result.tile_count = layout->hillshade_tiles.size();
    result.opacity = layout->hillshade_tiles.back().opacity;
    result.texture_key = layout->hillshade_tiles.back().texture_key;
  } else {
    result.kind = HillshadeAttachKind::kBakeFailed;
  }
  result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
  return result;
}

}  // namespace vista
