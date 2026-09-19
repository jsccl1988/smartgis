// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_MVT_STUB_H_
#define SDB_TILE_MVT_STUB_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gis/gis_export.h"
#include "gis/tile/style_source.h"

namespace gis {
namespace tile {
namespace mvt {

// MapLibre vector / MVT (PBF) decode — NON-GOAL for tile P3 (sources binding).
// Next phase: gzip+protobuf layer decode into feature batches for style rules.
// Callers must treat every API here as a hard failure, never as empty success.

enum class DecodeStatus {
  kNotImplemented = 1,
};

// Always returns false and kNotImplemented. |out_features| is left unchanged.
GIS_EXPORT bool decode_tile(const uint8_t* data, size_t len,
                            std::vector<std::string>* out_features);

GIS_EXPORT DecodeStatus decode_status();

// Convenience: Style source type=vector →
// StyleSourceStatus::kVectorUnsupported.
GIS_EXPORT StyleSourceStatus reject_vector_source();

GIS_EXPORT const char* non_goal_message();

}  // namespace mvt
}  // namespace tile
}  // namespace gis

#endif  // SDB_TILE_MVT_STUB_H_
