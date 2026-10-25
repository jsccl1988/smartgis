// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GPU draw cache for MapPass::record: world/overlay identity hashes, placed
// view latch, and keyed slice hits. No RHI encode.

#ifndef VISTA_PASS_MAP_DETAIL_DRAW_CACHE_H_
#define VISTA_PASS_MAP_DETAIL_DRAW_CACHE_H_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "vista/component/map/ir.h"
#include "vista/component/map/view.h"
#include "vista/pass/map/detail/upload.h"

namespace vista {
namespace detail {

// True when |a| and |b| share the same pixel size and lon/lat extent.
bool view_extent_matches(const vista::View& a, const vista::View& b);

// Same pixel size and world span (pure translation). Zoom changes span.
bool view_scale_matches(const vista::View& placed, const vista::View& view);

// Drop pixel-space text/icon (and non-DEM textured draws). Interactive
// world-only encode reuses lon/lat buffers under a live camera.
std::vector<UploadedDraw> world_only_draws(
    const std::vector<UploadedDraw>& draws);

// Drop overlays and DEM hillshade rasters (zoom / stale skirt UVs).
std::vector<UploadedDraw> world_without_dem_raster(
    const std::vector<UploadedDraw>& draws);

bool has_overlay_draws(const std::vector<UploadedDraw>& draws);

bool frame_has_overlay_items(const vista::MapIR& frame);

bool frame_has_screen_fixed_items(const vista::MapIR& frame);

std::vector<UploadedDraw> without_screen_fixed(
    const std::vector<UploadedDraw>& draws);

// FNV-1a over the world or overlay slice of |frame.items|.
uint64_t hash_items(const vista::MapIR& frame, bool overlay_slice);

// Owns uploaded draw lists between record() calls. Slice hits keep prior
// place() verts — clear them when the view extent moves.
struct DrawCache {
  struct SliceGpu {
    uint64_t hash = 0;
    std::vector<UploadedDraw> draws;
  };
  std::vector<UploadedDraw> world_draws;
  std::vector<UploadedDraw> full_draws;
  // View used when pixel_space overlays were placed into GPU verts.
  vista::View placed_view{};
  uint64_t world_hash = 0;
  uint64_t overlay_hash = 0;
  std::unordered_map<uint64_t, SliceGpu> slices;

  void clear_uploads();
  // Drop stale overlays when |view| changes scale. A pure pan (same span)
  // keeps them. Returns true when overlays remain valid for |view|.
  bool ensure_overlays_for_view(const vista::View& view);
};

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_MAP_DETAIL_DRAW_CACHE_H_
