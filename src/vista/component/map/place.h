// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU placement: MapIR items become ortho-space meshes. No RHI.

#ifndef VISTA_COMPONENT_MAP_PLACE_H_
#define VISTA_COMPONENT_MAP_PLACE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "vista/component/map/ir.h"

namespace vista {
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
//
// World-space solids may borrow DrawItem vertex/index vectors during
// place_frame. Keep MapIR alive through upload_draws (packs from these
// borrows into mega buffers). Call seal_borrowed_meshes only when MapIR
// may die before upload — FlyCube reads the packed mega spans, not the
// borrows, during Device::upload.
struct PlacedMesh {
  MeshSource source = MeshSource::kSolid;
  std::vector<vista::Vertex> vertices;
  std::vector<uint32_t> indices;
  const std::vector<vista::Vertex>* vertices_src = nullptr;
  const std::vector<uint32_t>* indices_src = nullptr;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;
  // 0 means this mesh is not tied to a glyph. Text body and halo share a group.
  uint32_t glyph_group = 0;
  GlyphId glyph;
  uint32_t raster_key = 0;
  // Hillshade rasters are kMultiply. Every other mesh stays kOver.
  vista::DrawBlend blend = vista::DrawBlend::kOver;
  std::string symbol_id;
  // Fill, line, and circle with a symbol id draw solid when the icon is
  // missing. Icons do not.
  bool solid_if_icon_missing = false;

  const std::vector<vista::Vertex>& vertex_list() const {
    return vertices_src ? *vertices_src : vertices;
  }
  const std::vector<uint32_t>& index_list() const {
    return indices_src ? *indices_src : indices;
  }
};

// Copy borrowed DrawItem spans into owned vertices/indices and drop the
// pointers. Idempotent. Safe to call while the MapIR is still alive.
void seal_borrowed_meshes(std::vector<PlacedMesh>* meshes);

// World fill/line/circle emit a one-pixel radial feather solid under the core
// mesh (Phase 2a AA). Rotates pixel-space icon and text about anchor_x/y,
// maps pixels into the lon/lat ortho, and emits halo quads. Raster/icon tint
// uses DrawItem rgba × opacity at encode time. A kMultiply raster keeps that
// opacity for coverage baking and draws with tint alpha 1.
// |world_items| keeps fill/line/circle/raster. |overlay_items| keeps icon/text.
// Both default on. Skip by kind instead of copying DrawItem into a subset —
// borrow pointers must address the caller's MapIR, not a temporary copy.
std::vector<PlacedMesh> place_frame(const vista::MapIR& frame,
                                    const vista::View& view,
                                    bool world_items = true,
                                    bool overlay_items = true);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_PLACE_H_
