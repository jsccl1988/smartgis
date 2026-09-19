// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_WMTS_H_
#define SDB_TILE_WMTS_H_

#include <string>

#include "gis/gis_export.h"

namespace gis {
namespace tile {

// Minimal WMTS helpers: parse ResourceURL / GetTile-style templates from a
// Capabilities document (or accept a hand-written template). Placeholders
// {TileMatrix}/{TileCol}/{TileRow} map to XYZ {z}/{x}/{y}.

// Rewrites WMTS placeholders to {z}/{x}/{y}. Returns false if neither
// WMTS nor XYZ placeholders are present.
GIS_EXPORT bool normalize_wmts_url_template(const std::string& in,
                                            std::string* out);

// Best-effort parse of WMTS GetCapabilities XML. Prefers the first
// ResourceURL template=...; falls back to a GetTile KVP pattern when
// OperationsMetadata + Layer Identifier are present.
GIS_EXPORT bool parse_wmts_capabilities(const std::string& xml,
                                        std::string* url_template,
                                        std::string* error);

}  // namespace tile
}  // namespace gis

#endif  // SDB_TILE_WMTS_H_
