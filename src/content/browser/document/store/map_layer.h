// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_STORE_MAP_LAYER_H_
#define CONTENT_BROWSER_DOCUMENT_STORE_MAP_LAYER_H_

#include <cstring>
#include <string>
#include <vector>

#include "content/public/map_layer_types.h"

namespace content {
namespace detail {

// Matches normal map content: area / line / point / annotation text.
enum class GeomKind { kPoint, kLine, kPolygon, kText };

// Map-space vertex (OGR / digitize). Not view pixels.
struct Vertex {
  double x = 0;
  double y = 0;
};

// One drawable feature in map space (owned by a MapLayer).
struct MapFeature {
  content::FeatureId id{};
  GeomKind kind = GeomKind::kPoint;
  std::vector<Vertex> points;
  std::vector<content::NamedField> fields;
  bool selected = false;
};

// Catalog-facing layer: id/name/visibility + features.
// |kind| is set at create/ingest when known; layer_descs may still infer
// kVector from non-empty |features| when kind stays kUnknown.
struct MapLayer {
  std::string id;
  std::string name;
  bool visible = true;
  content::LayerKind kind = content::LayerKind::kUnknown;
  std::vector<MapFeature> features;
};

inline const char* named_field_value(const MapFeature& f, const char* key) {
  if (!key) {
    return nullptr;
  }
  for (const content::NamedField& field : f.fields) {
    if (field.name == key) {
      return field.value.c_str();
    }
  }
  return nullptr;
}

inline bool feature_id_eq(const content::FeatureId& a,
                          const content::FeatureId& b) {
  return a.len == b.len &&
         std::memcmp(a.bytes, b.bytes, a.len) == 0;
}

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_STORE_MAP_LAYER_H_
