// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Pure CPU layout: style order in, MapFrame out. No RHI, HWND, or
// CameraMatrices. View::mode is a host hint; Layout does not branch on it.

#ifndef VISTA_MAP_LAYOUT_H_
#define VISTA_MAP_LAYOUT_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "gis/style/style_types.h"
#include "vista/map/batch.h"
#include "vista/map/draw.h"
#include "vista/map/view.h"
#include "vista/vista_export.h"

namespace vista {

// Injected glyph advances. Tests use a fixed-advance stub.
// Pixel rasterization stays in the render pass.
class GlyphMetrics {
 public:
  virtual ~GlyphMetrics() = default;
  virtual float advance_px(uint32_t codepoint, float text_size_px) const = 0;
};

// One raster tile whose corners are already in the view CRS (lon/lat).
struct TileSlot {
  double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  float opacity = 1.f;
  uint32_t texture_key = 0;
};

// Pixel size of an icon or fill-pattern image the render pass can sample.
struct SymbolAsset {
  std::string id;
  float width_px = 0;
  float height_px = 0;
};

// Retained DrawItem groups keyed by cache_key. find returns null when the
// slice missed, so emit tessellates it. A full rebuild passes null.
class SliceCache {
 public:
  virtual ~SliceCache() = default;
  virtual const std::vector<DrawItem>* find(uint64_t cache_key) const = 0;
};

// Inputs that are not per-feature. symbols is matched by id.
// hillshade_tiles: host-baked DEM shade underlay (texture_key + lon/lat).
struct LayoutInput {
  View view;
  const gis::style::StyleDocument* style = nullptr;
  double zoom = 0;
  std::vector<TileSlot> tiles;
  std::vector<TileSlot> hillshade_tiles;
  const GlyphMetrics* metrics = nullptr;
  std::vector<SymbolAsset> symbols;
  // Settle / C1: skip fill/line/circle/extrusion/heatmap/raster/hillshade and
  // emit only symbols (labels). Host copies world DrawItems from published.
  bool reuse_world_items = false;
  // Non-owning. Null means every cache_key misses (full emit).
  const SliceCache* retained_slices = nullptr;
  // Host stamp for this build. 0 = never abort mid-emit (tests / sync path).
  uint64_t layout_gen = 0;
  // Non-owning. When set, workers compare load() to layout_gen at emit batch
  // boundaries and skip remaining tess. The host must not publish a stale gen.
  const std::atomic<uint64_t>* live_layout_gen = nullptr;
};

class Layout {
 public:
  VISTA_EXPORT MapFrame build(const LayoutInput& in,
                            const std::vector<LayerBatch>& layers) const;
};

VISTA_EXPORT std::string default_carto_style_json();

// Default carto without symbol / label layers — print layout chrome uses this
// so city glyphs cannot twin-draw under MapLibre label slots.
VISTA_EXPORT std::string print_carto_style_json();

}  // namespace vista

#endif  // VISTA_MAP_LAYOUT_H_
