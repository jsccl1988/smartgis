// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/detail/draw_cache.h"

#include <cmath>

namespace vista {
namespace detail {

bool view_extent_matches(const vista::View& a, const vista::View& b) {
  constexpr double kEps = 1.0e-9;
  return a.width_px == b.width_px && a.height_px == b.height_px &&
         std::fabs(a.min_x - b.min_x) < kEps &&
         std::fabs(a.min_y - b.min_y) < kEps &&
         std::fabs(a.max_x - b.max_x) < kEps &&
         std::fabs(a.max_y - b.max_y) < kEps;
}

// Same pixel size and world span (pure translation). A zoom changes the span
// and must not reuse pixel-space glyphs placed at the previous scale.
bool view_scale_matches(const vista::View& placed, const vista::View& view) {
  if (placed.width_px == 0 || view.width_px == 0 ||
      placed.width_px != view.width_px ||
      placed.height_px != view.height_px) {
    return false;
  }
  const double ax = placed.max_x - placed.min_x;
  const double bx = view.max_x - view.min_x;
  const double ay = placed.max_y - placed.min_y;
  const double by = view.max_y - view.min_y;
  if (!(ax > 0.0) || !(ay > 0.0) || !(bx > 0.0) || !(by > 0.0)) {
    return false;
  }
  constexpr double kRel = 1.0e-4;
  return std::fabs(ax - bx) <= kRel * ax && std::fabs(ay - by) <= kRel * ay;
}

std::vector<UploadedDraw> world_only_draws(
    const std::vector<UploadedDraw>& draws) {
  std::vector<UploadedDraw> out;
  out.reserve(draws.size());
  for (const UploadedDraw& d : draws) {
    // Drop pixel-space text/icon (and any non-DEM textured draw). Interactive
    // world-only encode reuses these buffers under a live camera — stale
    // glyph quads read as huge characters while panning over hillshade.
    if (d.overlay || (d.textured && !d.dem_raster)) {
      continue;
    }
    out.push_back(d);
  }
  return out;
}

// Zoom changes world span. Hillshade quads were clipped/UV-mapped for the
// layout view (+ pan skirt); replaying them under a new ortho camera stretches
// the DEM sheet into horizontal gray bands (DX12 present).
std::vector<UploadedDraw> world_without_dem_raster(
    const std::vector<UploadedDraw>& draws) {
  std::vector<UploadedDraw> out;
  out.reserve(draws.size());
  for (const UploadedDraw& d : draws) {
    if (d.overlay || (d.textured && !d.dem_raster)) {
      continue;
    }
    if (d.textured && d.dem_raster) {
      continue;
    }
    out.push_back(d);
  }
  return out;
}

bool has_overlay_draws(const std::vector<UploadedDraw>& draws) {
  for (const UploadedDraw& d : draws) {
    if (d.overlay || (d.textured && !d.dem_raster)) {
      return true;
    }
  }
  return false;
}

bool frame_has_overlay_items(const vista::MapIR& frame) {
  for (const vista::DrawItem& item : frame.items) {
    if (item.kind == vista::DrawKind::kIcon ||
        item.kind == vista::DrawKind::kText) {
      return true;
    }
  }
  return false;
}

uint64_t hash_items(const vista::MapIR& frame, bool overlay_slice) {
  uint64_t h = 14695981039346656037ull;
  auto mix = [&](uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  for (const vista::DrawItem& item : frame.items) {
    const bool is_ov = item.kind == vista::DrawKind::kIcon ||
                       item.kind == vista::DrawKind::kText;
    if (is_ov != overlay_slice) {
      continue;
    }
    mix(static_cast<uint64_t>(item.kind));
    mix(item.rgba);
    mix(item.cache_key);
    mix(item.vertices.size());
    mix(item.indices.size());
    if (!item.vertices.empty()) {
      mix(static_cast<uint64_t>(
          static_cast<int32_t>(item.vertices.front().x * 1000.f)));
      mix(static_cast<uint64_t>(
          static_cast<int32_t>(item.vertices.back().y * 1000.f)));
    }
  }
  return h;
}

void DrawCache::clear_uploads() {
  world_draws.clear();
  full_draws.clear();
  world_hash = 0;
  overlay_hash = 0;
  slices.clear();
}

bool DrawCache::ensure_overlays_for_view(const vista::View& view) {
  if (view_extent_matches(placed_view, view)) {
    return true;
  }
  // Pan keeps world span. Glyph quads were placed into lon/lat at that
  // scale, so the live camera can move them without stretching.
  if (view_scale_matches(placed_view, view)) {
    return true;
  }
  // Zoom: drop stale overlays and DEM rasters tied to the old view skirt.
  if (!full_draws.empty()) {
    full_draws = world_without_dem_raster(full_draws);
  }
  if (!world_draws.empty()) {
    world_draws = world_without_dem_raster(world_draws);
  }
  // Slice hits keep place-time glyph verts — drop the index so Settle
  // cannot briefly replay overview-sized labels under the new camera.
  slices.clear();
  // Invalidate the latch until overlays are re-placed for |view|.
  placed_view = {};
  return false;
}

}  // namespace detail
}  // namespace vista
