// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Incremental upload by DrawItem.cache_key runs. Hits keep GPU buffers;
// misses place+upload a subset MapIR. Encode is the caller's job.

#ifndef VISTA_PASS_MAP_DETAIL_KEYED_SLICES_H_
#define VISTA_PASS_MAP_DETAIL_KEYED_SLICES_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "vista/component/map/ir.h"
#include "vista/pass/map/detail/draw_cache.h"
#include "vista/pass/map/detail/upload.h"

namespace vista {
class GlyphRasterizer;

namespace detail {

// Upload cache_key misses into |cache|, merge hits+misses into |out_merged|.
// Destroys stale slice GPU resources via |device|. Returns false only when
// |device| is null (caller still owns encode / pipeline ensure).
bool upload_keyed_slices(
    render::rhi::Device* device, DrawCache* cache,
    const vista::MapIR& frame, const vista::View& view,
    GlyphRasterizer* glyphs, bool world_items, bool overlay_items,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(const std::string& symbol_id,
                             std::vector<uint8_t>* rgba, int* w, int* h)>&
        load_icon,
    std::vector<render::rhi::Buffer*>* buffers,
    std::vector<render::rhi::Texture*>* textures,
    std::vector<UploadedDraw>* out_merged);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_MAP_DETAIL_KEYED_SLICES_H_
