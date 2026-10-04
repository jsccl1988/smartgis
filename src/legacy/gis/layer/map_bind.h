// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_LAYER_MAP_BIND_H_
#define SMT_LEGACY_GIS_LAYER_MAP_BIND_H_

#include "gis/map/map.h"
#include "legacy/gis/layer/layer.h"

namespace gis {

// Leftover adapters over product Map / MapLayer. Product TUs must not include
// this header.
GIS_EXPORT bool leftover_add_layer(Map* map, Layer* layer, bool owns = true);
GIS_EXPORT Layer* leftover_layer_at(Map* map, int index);
GIS_EXPORT const Layer* leftover_layer_at(const Map* map, int index);
GIS_EXPORT Layer* leftover_layer_named(Map* map, const char* name);
GIS_EXPORT Layer* leftover_active_layer(Map* map);

}  // namespace gis

#ifndef MAX_MAP_NAME
#define MAX_MAP_NAME gis::k_map_name_max
#endif

#endif  // SMT_LEGACY_GIS_LAYER_MAP_BIND_H_
