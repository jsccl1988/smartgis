// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_TILE_TILE_DISK_CACHE_PATHS_H_
#define GIS_TILE_TILE_DISK_CACHE_PATHS_H_

#include <cstdint>
#include <string>

#include "gis/gis_export.h"

namespace gis {
namespace tile {

// FNV-1a 64-bit — stable subdirectory name for a URL template.
GIS_EXPORT uint64_t url_template_hash64(const std::string& url_template);

// Root directory for XYZ/WMTS disk tiles: --tile-disk-cache= or
// <exe>/../data/cache/xyz_tiles (same layout as dem_bake under out/data/cache).
GIS_EXPORT std::string default_xyz_tile_cache_root();

// Per-template cache directory under the root (empty when root unavailable).
GIS_EXPORT std::string disk_cache_dir_for_template(
    const std::string& url_template);

}  // namespace tile
}  // namespace gis

#endif  // GIS_TILE_TILE_DISK_CACHE_PATHS_H_
