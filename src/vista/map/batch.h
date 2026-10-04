// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// POD layers → style source-layer batches. Does not read MapScene.
// LayerBatchSet destructor is out of line so OGRGeometry stays incomplete.

#ifndef VISTA_MAP_BATCH_H_
#define VISTA_MAP_BATCH_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "vista/vista_export.h"

class OGRGeometry;

namespace vista {

// Geometries that share one style source-layer, plus per-feature attributes.
struct LayerBatch {
  std::string source_layer;
  std::vector<const OGRGeometry*> geoms;
  std::vector<std::map<std::string, std::string>> attrs;
};

enum class BatchGeomKind : uint8_t { kPoint, kLine, kPolygon, kText };

struct BatchPoint {
  double x = 0;
  double y = 0;
};

struct BatchField {
  std::string name;
  std::string value;
};

struct BatchFeature {
  BatchGeomKind kind = BatchGeomKind::kPoint;
  std::vector<BatchPoint> points;
  std::vector<BatchField> fields;
};

struct BatchLayer {
  std::string id;
  std::string name;
  bool visible = true;
  std::vector<BatchFeature> features;
};

struct LayerBatchSet {
  std::vector<LayerBatch> batches;
  std::vector<std::unique_ptr<OGRGeometry>> owned;

  VISTA_EXPORT LayerBatchSet();
  VISTA_EXPORT ~LayerBatchSet();
  VISTA_EXPORT LayerBatchSet(LayerBatchSet&&) noexcept;
  VISTA_EXPORT LayerBatchSet& operator=(LayerBatchSet&&) noexcept;
  LayerBatchSet(const LayerBatchSet&) = delete;
  LayerBatchSet& operator=(const LayerBatchSet&) = delete;
};

VISTA_EXPORT LayerBatchSet build_layer_batches(const std::vector<BatchLayer>& layers,
                                             bool use_carto_slots,
                                             double scale = 0.0);

}  // namespace vista

#endif  // VISTA_MAP_BATCH_H_
