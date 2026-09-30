// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU placement: MapFrame items become ortho-space meshes. No RHI.

#ifndef EFFECT_MAP_DETAIL_PLACE_H_
#define EFFECT_MAP_DETAIL_PLACE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gis/vista/frame/frame.h"

namespace effect {
namespace map {
namespace detail {

// How a placed mesh is sampled after upload.
enum class MeshSource {
  kSolid,
  kGlyph,
  kRaster,
  kIcon,
};

// Unquantized identity of one atlas entry. The packer quantizes size and
// opacity; placement only copies the draw item fields.
struct GlyphId {
  uint32_t codepoint = 0;
  float text_size_px = 0.f;
  uint32_t rgba = 0;
  float opacity = 1.f;
};

// One drawable mesh in the view lon/lat ortho. Pixel-space icon and text are
// already rotated about the anchor. A text halo is a solid mesh that shares
// glyph_group with its glyph quad so a missing atlas entry drops both.
struct PlacedMesh {
  MeshSource source = MeshSource::kSolid;
  std::vector<gis::vista::Vertex> vertices;
  std::vector<uint32_t> indices;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;
  // 0 means this mesh is not tied to a glyph. Text body and halo share a group.
  uint32_t glyph_group = 0;
  GlyphId glyph;
  uint32_t raster_key = 0;
  std::string symbol_id;
  // Fill, line, and circle with a symbol id draw solid when the icon is
  // missing. Icons do not.
  bool solid_if_icon_missing = false;
};

// World fill/line/circle emit a one-pixel radial feather solid under the core
// mesh (Phase 2a AA). Rotates pixel-space icon and text about anchor_x/y,
// maps pixels into the lon/lat ortho, and emits halo quads. Raster/icon tint
// uses DrawItem rgba × opacity at encode time.
std::vector<PlacedMesh> place_frame(const gis::vista::MapFrame& frame,
                                    const gis::vista::View& view);

}  // namespace detail
}  // namespace map
}  // namespace effect

#endif  // EFFECT_MAP_DETAIL_PLACE_H_
