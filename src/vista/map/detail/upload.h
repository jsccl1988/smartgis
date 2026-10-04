// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// RHI upload for one record() call: buffers, the glyph atlas, raster tiles,
// and icons. Does not encode draws.

#ifndef EFFECT_MAP_DETAIL_UPLOAD_H_
#define EFFECT_MAP_DETAIL_UPLOAD_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "vista/map/detail/atlas.h"
#include "render/rhi/rhi.h"

namespace render {
namespace rhi {
class Buffer;
class Device;
class Texture;
}  // namespace rhi

}  // namespace render

namespace vista {
namespace detail {

// One draw range into a mega VB/IB (or a single-mesh buffer). Encoder binds
// vertices/indices and issues draw_indexed(index_count, …, first_index, …).
// Tint stays per-draw so solid/UV megas can pack meshes that share pipeline
// and texture binding but differ in color/opacity.
struct UploadedDraw {
  bool textured = false;
  render::rhi::Buffer* vertices = nullptr;
  render::rhi::Buffer* indices = nullptr;
  uint32_t index_count = 0;
  // Index of the first index in the shared IB (D3D12 / Vulkan firstIndex).
  uint32_t first_index = 0;
  uint32_t stride = 0;
  render::rhi::Texture* texture = nullptr;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;
  // Textured hillshade is kMultiply after the luma bake. Other draws stay
  // kSrcAlpha (solids ignore this and pick opaque vs src-alpha from |a|).
  render::rhi::BlendMode blend = render::rhi::BlendMode::kSrcAlpha;
};

// Creates buffers and textures owned by the caller (Pass). Missing raster or
// icon images skip that mesh. Raster keys and symbol ids are cached only for
// this call. A failed atlas upload drops every glyph group, halo included.
//
// Cold path: meshes that share a merge key (solid XYZ, or UV + same texture)
// pack into one VB+IB; painter order is preserved via first_index ranges.
// Packing pre-reserves each mega-buffer once (not per mesh) so china-scale
// appends stay linear.
std::vector<UploadedDraw> upload_draws(
    render::rhi::Device* device, const std::vector<PlacedMesh>& meshes,
    const AtlasLayout& atlas,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    std::vector<render::rhi::Buffer*>* buffers,
    std::vector<render::rhi::Texture*>* textures);

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_MAP_DETAIL_UPLOAD_H_
