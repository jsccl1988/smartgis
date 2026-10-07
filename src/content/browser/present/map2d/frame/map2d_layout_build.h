// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_

#include <atomic>
#include <cstdint>
#include <vector>

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "vista/component/map/ir.h"

namespace content {

class GisScene;
class ViewFrame;

namespace detail {

// Inputs for one L2 tess + optional DEM bake. All CPU work; no mu_.
struct Map2dLayoutParams {
  const GisScene* scene = nullptr;
  const ViewFrame* frame = nullptr;
  Map2dFrameCache::CameraKey cam;
  bool hillshade_ready = false;
  vista::TileSlot hillshade_slot{};
  const vista::SliceCache* retained_slices = nullptr;
  // First china layout defers the hillshade DrawItem unless force-shade.
  uint64_t layout_build_count = 0;
  uint64_t layout_gen = 0;
  const std::atomic<uint64_t>* live_layout_gen = nullptr;
};

struct Map2dLayoutOutput {
  vista::MapIR frame;
  std::vector<uint8_t> baked_rgba;
  int baked_w = 0;
  int baked_h = 0;
  vista::TileSlot hillshade_slot{};
  int64_t hillshade_ms = 0;
  // True when this build skipped DEM bake on purpose (first china layout).
  // Cache must force one follow-up rebuild so shade is not stuck off.
  bool deferred_hillshade = false;
  bool ok = false;
};

bool build_map2d_layout(const Map2dLayoutParams& in, Map2dLayoutOutput* out);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_
