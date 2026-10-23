// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/detail/place_upload.h"

#include "base/core/log.h"
#include "vista/component/map/place/place.h"
#include "vista/pass/map/detail/atlas.h"

namespace vista {
namespace detail {

std::vector<UploadedDraw> place_and_upload(
    render::rhi::Device* device, const vista::MapIR& frame,
    const vista::View& view, GlyphRasterizer* glyphs, bool world_items,
    bool overlay_items,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    std::vector<render::rhi::Buffer*>* buffers,
    std::vector<render::rhi::Texture*>* textures) {
  std::vector<PlacedMesh> meshes =
      place_frame(frame, view, world_items, overlay_items);
  size_t placed_glyphs = 0;
  for (const PlacedMesh& mesh : meshes) {
    if (mesh.source == MeshSource::kGlyph) {
      ++placed_glyphs;
    }
  }
  GlyphRasterizer* atlas_glyphs = overlay_items ? glyphs : nullptr;
  const AtlasLayout layout = pack_atlas(atlas_glyphs, frame);
  bind_glyph_layout(&meshes, layout);
  size_t kept_glyphs = 0;
  for (const PlacedMesh& mesh : meshes) {
    if (mesh.source == MeshSource::kGlyph) {
      ++kept_glyphs;
    }
  }
  // Guardrail: china-scale label frames used to discard the whole atlas when
  // the shelf was only 64px wide (height > 2048). Keep a failure breadcrumb.
  if (placed_glyphs > 0 && kept_glyphs == 0) {
    LOGGING(LOG_ERROR,
            "map2d.atlas dropped all glyphs placed=%zu cells=%zu atlas=%dx%d",
            placed_glyphs, layout.cells.size(), layout.width, layout.height);
  }
  return upload_draws(device, std::move(meshes), layout, load_raster, load_icon,
                      buffers, textures);
}

}  // namespace detail
}  // namespace vista
