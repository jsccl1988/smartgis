// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/catalog_layers.h"

namespace content {

std::string json_escape_string(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (char c : text) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string layers_to_catalog_json(const std::vector<LayerDesc>& layers) {
  std::string out = "[";
  bool first = true;
  for (const LayerDesc& layer : layers) {
    if (!first) {
      out += ',';
    }
    first = false;
    out += "{\"id\":\"";
    out += json_escape_string(layer.id);
    out += "\",\"name\":\"";
    out += json_escape_string(layer.name);
    out += "\",\"visible\":";
    out += layer.visible ? "true" : "false";
    out += '}';
  }
  out += ']';
  return out;
}

}  // namespace content
