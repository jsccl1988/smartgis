// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// RHI buffer / texture upload for one GpuScene::GpuMesh.

#ifndef EFFECT_SCENE_DETAIL_UPLOAD_H_
#define EFFECT_SCENE_DETAIL_UPLOAD_H_

#include <cstddef>
#include <cstdint>

#include "gis/map/map_layer.h"
#include "gis/carto/style/style_types.h"
#include "vista/world/world.h"
#include "render/rhi/rhi.h"
#include "vista/scene/scene.h"

namespace vista {
namespace detail {

constexpr uint32_t kPositionStride = 3 * sizeof(float);
constexpr uint32_t kLitPositionNormalStride = 6 * sizeof(float);

bool is_lit_kind(vista::NodeKind kind);

// SMT_SCENE3D_LIT_TERRAIN=1 enables Lambert for terrain / point-cloud.
// Default off: FlyCube lit normals have been painting pure-black overlays.
bool want_lit_terrain();

// True when rebuild should upload POSITION+NORMAL for this kind.
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

// Prefers in-memory bytes; path must already hold raw RGBA8 (no PNG decode).
render::rhi::Texture* upload_symbol_texture(
    render::rhi::Device* device, const gis::style::SymbolEntry& symbol);

bool upload_mesh(render::rhi::Device* device, const float* positions,
                 size_t position_count, const uint32_t* indices,
                 size_t index_count, bool with_uv, bool with_normals,
                 bool uv_on_xz, const float* explicit_uvs,
                 GpuScene::GpuMesh* out);

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_SCENE_DETAIL_UPLOAD_H_
