// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_FEATURE_LEFTOVER_COPY_LAYER_H_
#define SMT_LEGACY_GIS_FEATURE_LEFTOVER_COPY_LAYER_H_

#include "gis/gis_export.h"
#include "gis/feature/feature.h"
#include "legacy/gis/layer/layer.h"

inline long copy_layer(OGRLayer* dest, OGRLayer* src) {
  return gis::copy_ogr_layer(dest, src);
}

// Leftover Layer* / RasterLayer* copy. Product copy_layer is OGR-only.
long GIS_EXPORT copy_layer(gis::Layer* dest, gis::Layer* src,
                           bool clone = true, bool check_feature_type = false);
long GIS_EXPORT copy_layer(gis::RasterLayer* dest, gis::RasterLayer* src,
                           bool clone = true, bool check_feature_type = false);

#endif  // SMT_LEGACY_GIS_FEATURE_LEFTOVER_COPY_LAYER_H_
