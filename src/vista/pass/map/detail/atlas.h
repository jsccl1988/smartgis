// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU glyph atlas: rects and tinted RGBA for the codepoints a frame draws.

#ifndef VISTA_PASS_MAP_DETAIL_ATLAS_H_
#define VISTA_PASS_MAP_DETAIL_ATLAS_H_

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "vista/component/map/place.h"

namespace vista {
class GlyphRasterizer;

namespace detail {

// Quantized identity of a tinted glyph. Equal keys share one atlas cell.
struct GlyphKey {
  uint32_t codepoint = 0;
  uint32_t size_q = 0;
  uint32_t rgba = 0;
  uint32_t opacity_q = 0;

  bool operator==(const GlyphKey& other) const {
    return codepoint == other.codepoint && size_q == other.size_q &&
           rgba == other.rgba && opacity_q == other.opacity_q;
  }
};

// Mixes the quantized fields for the atlas cell map.
struct GlyphKeyHash {
  size_t operator()(const GlyphKey& key) const noexcept {
    size_t h = static_cast<size_t>(key.codepoint);
    auto mix = [&h](uint32_t v) {
      h ^= static_cast<size_t>(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
    };
    mix(key.size_q);
    mix(key.rgba);
    mix(key.opacity_q);
    return h;
  }
};

// Pixel rect of one glyph inside the atlas, origin at the top-left.
struct GlyphCell {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// Shelf-packed glyph image for one frame. No GPU texture.
struct AtlasLayout {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
  std::unordered_map<GlyphKey, GlyphCell, GlyphKeyHash> cells;
};

// Rasters the codepoints this frame actually draws. A failed GlyphRasterizer
// omits that glyph. No RHI.
AtlasLayout pack_atlas(GlyphRasterizer* glyphs,
                       const vista::MapIR& frame);

// Writes atlas-cell UVs onto glyph meshes. Drops a text group whose glyph is
// missing, including its halo.
void bind_glyph_layout(std::vector<PlacedMesh>* meshes,
                       const AtlasLayout& layout);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_MAP_DETAIL_ATLAS_H_
