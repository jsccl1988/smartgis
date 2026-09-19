// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ASSETS_TILESET_H_
#define GIS_ASSETS_TILESET_H_

#include <cstddef>
#include <string>
#include <vector>

#include "gis/gis_export.h"

// Explicit 3D Tiles 1.0/1.1 tileset.json. Streaming selection is CPU-side.

namespace gis {

enum class Refine { kReplace, kAdd };

struct Tile {
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  double geometric_error;
  Refine refine;
  std::string content_uri;
  std::vector<Tile> children;

  Tile()
      : min_x(0),
        min_y(0),
        min_z(0),
        max_x(0),
        max_y(0),
        max_z(0),
        geometric_error(0),
        refine(Refine::kReplace) {}
};

struct Tileset {
  Tile root;
};

struct ViewState {
  double eye_x;
  double eye_y;
  double eye_z;
  double sse_denominator;
};

GIS_EXPORT bool parse_tileset_json(const char* json, size_t len, Tileset& out);
GIS_EXPORT void select_tiles(const Tileset& tileset, const ViewState& view,
                             double max_sse, std::vector<const Tile*>& visible);

}  // namespace gis

#endif  // GIS_ASSETS_TILESET_H_
