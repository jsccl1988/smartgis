// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "vista/map/ir.h"

namespace base {
struct Arena;
}

namespace content {

class MapScene;
class ViewFrame;

namespace detail {

// Inputs for one L2 tess + optional DEM bake. All CPU work; no mu_.
struct Map2dLayoutParams {
  const MapScene* scene = nullptr;
  const ViewFrame* frame = nullptr;
  Map2dFrameCache::CameraKey cam;
  Map2dFrameCache::PresentAction action = Map2dFrameCache::PresentAction::kRebuildFull;
  std::shared_ptr<const vista::MapIR> prev_published;
  bool hillshade_ready = false;
  vista::TileSlot hillshade_slot{};
  base::Arena* scratch = nullptr;
  uint64_t layout_gen = 0;
  const std::atomic<uint64_t>* live_layout_gen = nullptr;
};

struct Map2dLayoutOutput {
  vista::MapIR frame;
  std::shared_ptr<const std::vector<uint8_t>> baked_rgba;
  int baked_w = 0;
  int baked_h = 0;
  vista::TileSlot hillshade_slot{};
  int64_t hillshade_ms = 0;
  bool ok = false;
};

bool build_map2d_layout(const Map2dLayoutParams& in, Map2dLayoutOutput* out);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_LAYOUT_BUILD_H_
