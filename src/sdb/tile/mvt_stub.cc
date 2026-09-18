// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/mvt_stub.h"

namespace sdb {
namespace tile {
namespace mvt {

bool decode_tile(const uint8_t* /*data*/,
                 size_t /*len*/,
                 std::vector<std::string>* /*out_features*/) {
  return false;
}

DecodeStatus decode_status() {
  return DecodeStatus::kNotImplemented;
}

StyleSourceStatus reject_vector_source() {
  return StyleSourceStatus::kVectorUnsupported;
}

const char* non_goal_message() {
  return "MVT/vector source decode is out of scope for tile P3; "
         "bind raster XYZ sources only. Next phase: protobuf+gzip MVT.";
}

}  // namespace mvt
}  // namespace tile
}  // namespace sdb
