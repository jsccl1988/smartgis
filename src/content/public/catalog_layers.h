// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_CATALOG_LAYERS_H_
#define CONTENT_PUBLIC_CATALOG_LAYERS_H_

#include <string>
#include <string_view>
#include <vector>

#include "content/content_export.h"

namespace content {

// Catalog / LayerTree node kind. Hosts may leave kUnknown for flat mirrors.
enum class LayerKind {
  kUnknown = 0,
  kGroup,
  kVector,
  kRaster,
};

// Opaque Catalog / LayerTree row for shell mirrors. No GIS pointers, no HWND.
// Nested |children| are optional; empty children keep flat-list wire callers.
struct LayerDesc {
  std::string id;
  std::string name;
  bool visible = true;
  bool active = false;
  LayerKind kind = LayerKind::kUnknown;
  bool expanded = true;
  std::vector<LayerDesc> children;
};

// Escape a string for embedding inside a JSON double-quoted value.
CONTENT_EXPORT std::string json_escape_string(std::string_view text);

// Serialize |layers| to a JSON array consumed by CEF CatalogDelta / LegendSnapshot:
// [{"id":"...","name":"...","visible":true}, ...]
// |active|, |kind|, |expanded|, and |children| are intentionally omitted to
// match the existing CEF wire format (flat id/name/visible only).
CONTENT_EXPORT std::string layers_to_catalog_json(
    const std::vector<LayerDesc>& layers);

}  // namespace content

#endif  // CONTENT_PUBLIC_CATALOG_LAYERS_H_
