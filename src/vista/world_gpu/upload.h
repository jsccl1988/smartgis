// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// RHI buffer / texture upload for one GpuMesh.

#ifndef VISTA_WORLD_GPU_UPLOAD_H_
#define VISTA_WORLD_GPU_UPLOAD_H_

#include <cstddef>
#include <cstdint>

#include "gis/map/map_layer.h"
#include "gis/style/style_types.h"
#include "render/rhi/rhi.h"
#include "vista/world_gpu/gpu_mesh.h"
#include "vista/world/world.h"

namespace vista {
namespace detail {

constexpr uint32_t kPositionStride = 3 * sizeof(float);
constexpr uint32_t kLitPositionNormalStride = 6 * sizeof(float);

bool is_lit_kind(vista::NodeKind kind);

bool want_lit_terrain();

bool upload_with_normals(vista::NodeKind kind, bool with_uv);

render::rhi::TextureDesc texture_desc_from_bytes(uint32_t byte_size);

bool layer_image_pixels(const gis::MapLayer* layer, const void** data,
                        uint32_t* byte_size);

render::rhi::Texture* upload_rgba_texture(render::rhi::Device* device,
                                          const void* pixels,
                                          uint32_t byte_size);

render::rhi::Texture* upload_rgba_texture_wh(render::rhi::Device* device,
                                             const void* pixels, uint32_t width,
                                             uint32_t height);

render::rhi::Texture* upload_layer_texture(render::rhi::Device* device,
                                           const gis::MapLayer* layer);

render::rhi::Texture* upload_symbol_texture(
    render::rhi::Device* device, const gis::style::SymbolEntry& symbol);

bool upload_mesh(render::rhi::Device* device, const float* positions,
                 size_t position_count, const uint32_t* indices,
                 size_t index_count, bool with_uv, bool with_normals,
                 bool uv_on_xz, const float* explicit_uvs, GpuMesh* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_WORLD_GPU_UPLOAD_H_
