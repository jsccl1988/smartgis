// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/feature/leftover_copy_layer.h"

#include "legacy/core/macros/macros.h"

long copy_layer(gis::Layer* dest, gis::Layer* src, bool clone,
                bool check_feature_type) {
  if (!dest || !src || dest->GetLayerType() != src->GetLayerType()) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (src->GetLayerType() == gis::LYR_RASTER) {
    return copy_layer(static_cast<gis::RasterLayer*>(dest),
                      static_cast<gis::RasterLayer*>(src), clone,
                      check_feature_type);
  }
  return SMT_ERR_FAILURE;
}

long copy_layer(gis::RasterLayer* dest, gis::RasterLayer* src,
                bool /*clone*/, bool /*check_feature_type*/) {
  if (!dest || !src) {
    return SMT_ERR_INVALID_PARAM;
  }
  char* raster_buf = nullptr;
  long raster_buf_size = 0;
  long code_type = -1;
  fRect loc_rect;
  if (SMT_ERR_NONE == src->GetRasterNoClone(raster_buf, raster_buf_size,
                                            loc_rect, code_type) &&
      SMT_ERR_NONE == dest->CreaterRaster(raster_buf, raster_buf_size, loc_rect,
                                          code_type)) {
    dest->CalEnvelope();
    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}
