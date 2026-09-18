// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_STYLE_TYPES_H_
#define SDB_STYLE_STYLE_TYPES_H_

#include "sdb/gis_export.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace sdb {
namespace style {

// MapLibre-inspired layer type (v1+ subset; extrusion/heatmap/hillshade are
// parse placeholders — no render path yet).
enum class LayerType {
  kUnknown = 0,
  kFill,
  kLine,
  kSymbol,
  kCircle,
  kBackground,
  kRaster,
  kFillExtrusion,
  kHeatmap,
  kHillshade,
};

// Filter AST operator (MapLibre filter array form).
enum class FilterOp {
  kTrue = 0,  // empty / missing filter matches
  kEq,
  kNeq,
  kLt,
  kLte,
  kGt,
  kGte,
  kHas,
  kNotHas,
  kIn,
  kNotIn,
  kAll,
  kAny,
  kNone,
};

// One node in a MapLibre-style filter expression tree.
struct FilterNode {
  FilterOp op = FilterOp::kTrue;
  std::string key;
  std::string value;
  std::vector<std::string> values;
  std::vector<FilterNode> children;
};

// One style layer entry from Style JSON.
struct StyleLayer {
  std::string id;
  LayerType type = LayerType::kUnknown;
  std::string source;
  std::string source_layer;
  double minzoom = 0;
  double maxzoom = 24;
  bool has_minzoom = false;
  bool has_maxzoom = false;
  FilterNode filter;
  std::map<std::string, std::string> paint;
  std::map<std::string, std::string> layout;
};

// Parsed Style Document (MapLibre Style Spec subset).
struct StyleDocument {
  int version = 0;
  std::string name;
  std::string sprite;
  std::string glyphs;
  std::vector<StyleLayer> layers;
};

// External symbol asset resolved by id.
struct SymbolEntry {
  std::string id;
  std::string path;
  std::vector<uint8_t> bytes;
};

// Paint parameters after rule evaluation (render-facing; not a Skia type).
struct ResolvedPaint {
  std::string layer_id;
  LayerType type = LayerType::kUnknown;
  uint32_t fill_color = 0xFF000000;
  float fill_opacity = 1.f;
  std::string fill_pattern;
  uint32_t line_color = 0xFF000000;
  float line_width = 1.f;
  float line_opacity = 1.f;
  std::vector<float> line_dasharray;
  std::string line_cap;
  std::string line_join;
  uint32_t circle_color = 0xFF000000;
  float circle_radius = 5.f;
  float circle_opacity = 1.f;
  std::string icon_image;
  std::string text_field;
  float icon_size = 1.f;
  float text_size = 16.f;
  std::string text_anchor;
  float icon_offset_x = 0.f;
  float icon_offset_y = 0.f;
  uint32_t background_color = 0xFF000000;
  float background_opacity = 1.f;
  float raster_opacity = 1.f;
  SymbolEntry symbol;
  bool has_symbol = false;
};

using AttrMap = std::map<std::string, std::string>;

GIS_EXPORT LayerType layer_type_from_string(const std::string& s);
GIS_EXPORT const char* layer_type_to_string(LayerType t);

}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_STYLE_TYPES_H_
