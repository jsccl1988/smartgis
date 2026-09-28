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

#include "effect/map/detail/atlas.h"

namespace render {
namespace rhi {
class Buffer;
class Device;
class Texture;
}  // namespace rhi

}  // namespace render

namespace effect {
namespace map {
namespace detail {

// Device buffers for one mesh. The encoder binds these and does not create them.
struct UploadedDraw {
  bool textured = false;
  render::rhi::Buffer* vertices = nullptr;
  render::rhi::Buffer* indices = nullptr;
  uint32_t index_count = 0;
  uint32_t stride = 0;
  render::rhi::Texture* texture = nullptr;
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;
};

// Creates buffers and textures owned by the caller (Pass). Missing raster or
// icon images skip that mesh. Raster keys and symbol ids are cached only for
// this call. A failed atlas upload drops every glyph group, halo included.
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
}  // namespace map
}  // namespace effect

#endif  // EFFECT_MAP_DETAIL_UPLOAD_H_
