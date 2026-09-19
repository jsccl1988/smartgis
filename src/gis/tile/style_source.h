// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_STYLE_SOURCE_H_
#define SDB_TILE_STYLE_SOURCE_H_

#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/map/map_layer.h"
#include "gis/tile/tile_provider.h"

namespace gis {
namespace tile {

// MapLibre Style Spec source "type" subset used by the tile data plane.
enum class StyleSourceType {
  kUnknown = 0,
  kRaster,
  kVector,       // recognized but not decoded (see mvt_stub.h)
  kUnsupported,  // geojson / image / video / …
};

// Result of parsing or binding a Style `sources` entry.
enum class StyleSourceStatus {
  kOk = 0,
  kInvalidJson,
  kNotObject,
  kMissingId,
  kMissingType,
  kMissingTiles,
  kEmptyTiles,
  kBadUrlTemplate,
  kVectorUnsupported,
  kUnsupportedType,
};

// Lightweight MapLibre-aligned source description (raster subset + typed
// stubs). Corresponds to one entry under Style JSON `sources` (id is the map
// key). StyleDocument today stores layer.source as a string only; this type
// owns the data-plane fields (tiles / tileSize) needed to open TileProvider.
struct StyleSourceDesc {
  std::string id;
  StyleSourceType type = StyleSourceType::kUnknown;
  std::vector<std::string> tiles;
  int tile_size = 256;
  int minzoom = 0;
  int maxzoom = 22;
  bool has_minzoom = false;
  bool has_maxzoom = false;

  // First tiles[] URL template — input to TileProvider::open_xyz /
  // make_xyz_map_layer. Empty when tiles is empty. Inline so tile_test and
  // other DLL consumers do not need an exported out-of-line member.
  const std::string& primary_url_template() const {
    static const std::string k_empty;
    return tiles.empty() ? k_empty : tiles.front();
  }
};

GIS_EXPORT const char* style_source_type_name(StyleSourceType t);
GIS_EXPORT const char* style_source_status_name(StyleSourceStatus s);

// Parse one source object JSON (without the id key). |id| is the sources map
// key. On kVectorUnsupported, *out still receives type=kVector when possible.
GIS_EXPORT StyleSourceStatus parse_style_source(const std::string& id,
                                                const char* json, size_t len,
                                                StyleSourceDesc* out);
GIS_EXPORT StyleSourceStatus parse_style_source(const std::string& id,
                                                const std::string& json,
                                                StyleSourceDesc* out);

// Parse Style root JSON, a `{"sources":{…}}` fragment, or a bare sources map
// object. Raster entries are appended to |out|. If any vector source is
// present, returns kVectorUnsupported after filling prior rasters (and does
// not pretend to decode MVT). Other unsupported types → kUnsupportedType
// (same fill-then-status rule). Pure raster docs return kOk.
GIS_EXPORT StyleSourceStatus parse_style_sources(
    const char* json, size_t len, std::vector<StyleSourceDesc>* out);
GIS_EXPORT StyleSourceStatus
parse_style_sources(const std::string& json, std::vector<StyleSourceDesc>* out);

// True when type is raster and primary URL has {z}/{x}/{y}.
GIS_EXPORT bool is_raster_bindable(const StyleSourceDesc& desc);

// Open TileProvider from a bindable raster desc (uses primary_url_template).
GIS_EXPORT bool open_provider_from_source(const StyleSourceDesc& desc,
                                          TileProvider* provider);

// Convenience: raster desc → MapLayer(kind=tile); empty layer on failure.
GIS_EXPORT MapLayer make_xyz_map_layer_from_source(const StyleSourceDesc& desc);

}  // namespace tile
}  // namespace gis

#endif  // SDB_TILE_STYLE_SOURCE_H_
