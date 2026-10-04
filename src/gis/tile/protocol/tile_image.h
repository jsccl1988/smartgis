// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_CARTO_TILE_PROTOCOL_TILE_IMAGE_H_
#define GIS_CARTO_TILE_PROTOCOL_TILE_IMAGE_H_

#include <string>

#include "gis/tile/protocol/xyz_math.h"

namespace gis {
namespace tile {

// One fetched XYZ tile: encoded image bytes + Web Mercator world rect.
struct TileImage {
  TileCoord coord;
  std::string bytes;
  Envelope world_rect{};
  long image_code = 4;  // PNG
};

}  // namespace tile
}  // namespace gis

#endif  // GIS_CARTO_TILE_PROTOCOL_TILE_IMAGE_H_
