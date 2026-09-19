// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_CATALOG_LAYERS_H_
#define CONTENT_PUBLIC_CATALOG_LAYERS_H_

#include <string>
#include <string_view>
#include <vector>

namespace content {

// Opaque Catalog / LayerTree row for chrome mirrors. No GIS pointers, no HWND.
struct LayerDesc {
  std::string id;
  std::string name;
  bool visible = true;
  bool active = false;
};

// Escape a string for embedding inside a JSON double-quoted value.
std::string json_escape_string(std::string_view text);

// Serialize |layers| to a JSON array consumed by CEF CatalogDelta / LegendSnapshot:
// [{"id":"...","name":"...","visible":true}, ...]
// |active| is intentionally omitted to match the existing CEF wire format.
std::string layers_to_catalog_json(const std::vector<LayerDesc>& layers);

}  // namespace content

#endif  // CONTENT_PUBLIC_CATALOG_LAYERS_H_
