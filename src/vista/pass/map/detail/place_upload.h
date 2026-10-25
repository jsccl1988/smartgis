// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One place → atlas → upload step for a MapIR subset. No encode.

#ifndef VISTA_PASS_MAP_DETAIL_PLACE_UPLOAD_H_
#define VISTA_PASS_MAP_DETAIL_PLACE_UPLOAD_H_

#include <functional>
#include <string>
#include <vector>

#include "vista/component/map/ir.h"
#include "vista/pass/map/detail/upload.h"

namespace vista {
class GlyphRasterizer;

namespace detail {

// place_frame + pack_atlas + bind_glyph_layout + upload_draws.
// |glyphs| is ignored when |overlay_items| is false (atlas skipped).
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
    std::vector<render::rhi::Texture*>* textures);

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
    std::vector<render::rhi::Texture*>* textures,
    const std::function<bool(uint32_t texture_key, const uint8_t** rgba, int* w,
                             int* h)>& borrow_raster);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_PASS_MAP_DETAIL_PLACE_UPLOAD_H_
