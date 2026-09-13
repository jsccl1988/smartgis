// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_TILE_MAP_LAYER_H_
#define SDB_TILE_TILE_MAP_LAYER_H_

#include "sdb/map/map_layer.h"
#include "sdb/tile/tile_provider.h"

#include "sdb/gis_export.h"

#include <memory>
#include <string>

namespace sdb {
namespace tile {

// MapLayer(kind=tile) hang path. Named make_map_layer (not MapLayer::static)
// to avoid a gis↔tile GN cycle; equivalent to composition-spec
// MapLayer::from_tile_provider.
GIS_EXPORT MapLayer make_map_layer(std::shared_ptr<TileProvider> provider);

// Convenience for Views / host "Add online basemap" flows.
// Returns an empty MapLayer (default) if the template is rejected.
GIS_EXPORT MapLayer make_xyz_map_layer(const std::string& url_template);

// WMTS URL template ({TileMatrix}/{TileCol}/{TileRow} or XYZ) → MapLayer.
GIS_EXPORT MapLayer make_wmts_map_layer(const std::string& url_template);

// Parse WMTS GetCapabilities XML (fixture or downloaded) → MapLayer.
GIS_EXPORT MapLayer make_wmts_map_layer_from_capabilities(
    const std::string& xml);

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_TILE_MAP_LAYER_H_
