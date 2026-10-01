// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_MVT_STUB_H_
#define SDB_TILE_MVT_STUB_H_

// Compatibility shim: MVT decode lives in mvt.h (no longer a stub).
#include "gis/present/tile/provider/mvt.h"

namespace gis {
namespace tile {
namespace mvt {

using ::gis::tile::DecodeStatus;
using ::gis::tile::decode_status;
using ::gis::tile::decode_tile;
using ::gis::tile::non_goal_message;
using ::gis::tile::reject_vector_source;

}  // namespace mvt
}  // namespace tile
}  // namespace gis

#endif  // SDB_TILE_MVT_STUB_H_
