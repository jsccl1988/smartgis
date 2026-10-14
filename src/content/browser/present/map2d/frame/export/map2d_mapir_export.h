// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_EXPORT_MAP2D_MAPIR_EXPORT_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_EXPORT_MAP2D_MAPIR_EXPORT_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "content/browser/present/map2d/frame/cache/map2d_frame_cache.h"
#include "vista/component/map/ir.h"

namespace content {
namespace detail {

// Raster loader for hillshade / textured MapIR items (texture_key → RGBA8).
using MapIrRasterLoader =
    std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
                       int* h)>;

// Faithful MapIR → BMP for product export / showcase (not scenic flat fill).
// Paints fill / line / raster under the same camera as MapPass.
bool export_mapir_bmp(const vista::MapIR& frame,
                      const Map2dFrameCache::CameraKey& cam,
                      const MapIrRasterLoader& load_raster,
                      const std::string& path);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_EXPORT_MAP2D_MAPIR_EXPORT_H_
