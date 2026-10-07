// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Vector-tile bytes become owned OGR batches and a MapIR. Decode stays in gis/tile.

#ifndef VISTA_COMPONENT_MAP_MVT_MVT_H_
#define VISTA_COMPONENT_MAP_MVT_MVT_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "gis/tile/provider/mvt.h"
#include "vista/component/map/ir.h"
#include "vista/vista_export.h"

class OGRGeometry;

namespace vista {

// Map tile-local rings into world CRS using the tile envelope, build owned
// OGR geometries + LayerBatches suitable for Layout::build. |holder| owns the
// OGRGeometry pointers referenced by |batches|.
VISTA_EXPORT bool mvt_to_layer_batches(
    const gis::tile::MvtTile& tile, double min_x, double min_y, double max_x,
    double max_y, std::vector<std::unique_ptr<OGRGeometry>>* holder,
    std::vector<LayerBatch>* batches);

// Decode + layout into a MapIR. |style_json| may be null/empty to use a
// minimal line/fill/circle style matching decoded layer names.
VISTA_EXPORT bool decode_mvt_to_map_frame(
    const uint8_t* data, size_t len, const View& view, double zoom,
    const char* style_json, MapIR* out_frame,
    gis::tile::MvtDecodeStatus* out_status);

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_MVT_MVT_H_
