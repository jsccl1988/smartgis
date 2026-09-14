// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/style/style_types.h"

namespace sdb {
namespace style {

LayerType layer_type_from_string(const std::string& s) {
  if (s == "fill") {
    return LayerType::kFill;
  }
  if (s == "line") {
    return LayerType::kLine;
  }
  if (s == "symbol") {
    return LayerType::kSymbol;
  }
  if (s == "circle") {
    return LayerType::kCircle;
  }
  return LayerType::kUnknown;
}

const char* layer_type_to_string(LayerType t) {
  switch (t) {
    case LayerType::kFill:
      return "fill";
    case LayerType::kLine:
      return "line";
    case LayerType::kSymbol:
      return "symbol";
    case LayerType::kCircle:
      return "circle";
    default:
      return "unknown";
  }
}

}  // namespace style
}  // namespace sdb
