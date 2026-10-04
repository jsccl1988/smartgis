// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_PRESENT_TILE_PROVIDER_MVT_H_
#define GIS_PRESENT_TILE_PROVIDER_MVT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gis/gis_export.h"
#include "gis/carto/tile/style_source.h"

namespace gis {
namespace tile {

// Mapbox Vector Tile (MVT / PBF) decode. Accepts raw protobuf or gzip-wrapped
// bytes. Unsupported encodings fail with an explicit status — never an empty
// success.

enum class MvtGeomType : uint8_t {
  kUnknown = 0,
  kPoint = 1,
  kLineString = 2,
  kPolygon = 3,
};

enum class MvtDecodeStatus : uint8_t {
  kOk = 0,
  kBadInput = 1,
  kUnsupportedEncoding = 2,
  kEmpty = 3,
};

// One decoded feature in tile-local coordinates (0..extent).
struct MvtFeature {
  uint64_t id = 0;
  MvtGeomType type = MvtGeomType::kUnknown;
  // Rings: Point/LineString use one ring; Polygon may use several (outer first).
  std::vector<std::vector<std::pair<double, double>>> rings;
  std::map<std::string, std::string> attrs;
};

struct MvtLayer {
  std::string name;
  uint32_t extent = 4096;
  uint32_t version = 1;
  std::vector<MvtFeature> features;
};

struct MvtTile {
  std::vector<MvtLayer> layers;
};

// Decode raw or gzip MVT into |out|. Returns kOk when at least one layer is
// present; kEmpty when the tile parses but has no layers/features.
GIS_EXPORT MvtDecodeStatus decode_mvt(const uint8_t* data, size_t len,
                                      MvtTile* out);

// Legacy summary API (feature strings like "road:linestring"). False on error.
GIS_EXPORT bool decode_tile(const uint8_t* data, size_t len,
                            std::vector<std::string>* out_features);

GIS_EXPORT MvtDecodeStatus decode_status_of(const uint8_t* data, size_t len);

// Compatibility alias used by older tests.
using DecodeStatus = MvtDecodeStatus;
GIS_EXPORT DecodeStatus decode_status();

// Style source type=vector still rejects network bind until a provider exists.
GIS_EXPORT StyleSourceStatus reject_vector_source();

GIS_EXPORT const char* non_goal_message();

}  // namespace tile
}  // namespace gis

#endif  // GIS_PRESENT_TILE_PROVIDER_MVT_H_
