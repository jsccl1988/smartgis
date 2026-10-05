// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MAP_LAYER_KIND_H_
#define GIS_MAP_LAYER_KIND_H_

// Product MapLayer kinds. Leftover virtual Layer / RasterLayer / TileLayer
// stay in leftover/gis/layer/layer.h.

namespace gis {

enum class LayerType {
  kVector,
  kRaster,
  kTile,
};

// Extra vector schemas that are not OGRwkbGeometryType. Product Feature
// geometry is OGR-only; these live on the layer / SDBD wrapper.
enum class VectorSchema {
  kNone = 0,
  kAnno,
  kChildImage,
  kGrid,
  kTin,
};

}  // namespace gis

#endif  // GIS_MAP_LAYER_KIND_H_
