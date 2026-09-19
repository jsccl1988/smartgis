// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/style/style_rules.h"

#include <cstdlib>

#include "gis/style/paint_resolve.h"

namespace gis {
namespace style {
namespace {

double as_number(const std::string& s, bool* ok) {
  char* stop = nullptr;
  double v = std::strtod(s.c_str(), &stop);
  *ok = stop != s.c_str();
  return v;
}

bool compare_values(FilterOp op, const std::string& left,
                    const std::string& right) {
  bool lok = false;
  bool rok = false;
  const double ln = as_number(left, &lok);
  const double rn = as_number(right, &rok);
  if (lok && rok) {
    switch (op) {
      case FilterOp::kEq:
        return ln == rn;
      case FilterOp::kNeq:
        return ln != rn;
      case FilterOp::kLt:
        return ln < rn;
      case FilterOp::kLte:
        return ln <= rn;
      case FilterOp::kGt:
        return ln > rn;
      case FilterOp::kGte:
        return ln >= rn;
      default:
        break;
    }
  }
  switch (op) {
    case FilterOp::kEq:
      return left == right;
    case FilterOp::kNeq:
      return left != right;
    case FilterOp::kLt:
      return left < right;
    case FilterOp::kLte:
      return left <= right;
    case FilterOp::kGt:
      return left > right;
    case FilterOp::kGte:
      return left >= right;
    default:
      return false;
  }
}

}  // namespace

bool layer_matches_zoom(const StyleLayer& layer, double zoom) {
  if (layer.has_minzoom && zoom < layer.minzoom) {
    return false;
  }
  if (layer.has_maxzoom && zoom >= layer.maxzoom) {
    return false;
  }
  return true;
}

bool eval_filter(const FilterNode& filter, const AttrMap& attrs) {
  switch (filter.op) {
    case FilterOp::kTrue:
      return true;
    case FilterOp::kAll:
      for (const auto& c : filter.children) {
        if (!eval_filter(c, attrs)) {
          return false;
        }
      }
      return true;
    case FilterOp::kAny:
      for (const auto& c : filter.children) {
        if (eval_filter(c, attrs)) {
          return true;
        }
      }
      return filter.children.empty();
    case FilterOp::kNone:
      for (const auto& c : filter.children) {
        if (eval_filter(c, attrs)) {
          return false;
        }
      }
      return true;
    case FilterOp::kHas:
      return attrs.find(filter.key) != attrs.end();
    case FilterOp::kNotHas:
      return attrs.find(filter.key) == attrs.end();
    case FilterOp::kIn: {
      auto it = attrs.find(filter.key);
      if (it == attrs.end()) {
        return false;
      }
      for (const auto& v : filter.values) {
        if (it->second == v) {
          return true;
        }
      }
      return false;
    }
    case FilterOp::kNotIn:
      return !eval_filter(
          FilterNode{FilterOp::kIn, filter.key, {}, filter.values, {}}, attrs);
    case FilterOp::kEq:
    case FilterOp::kNeq:
    case FilterOp::kLt:
    case FilterOp::kLte:
    case FilterOp::kGt:
    case FilterOp::kGte: {
      auto it = attrs.find(filter.key);
      if (it == attrs.end()) {
        return false;
      }
      return compare_values(filter.op, it->second, filter.value);
    }
    default:
      return true;
  }
}

std::vector<const StyleLayer*> select_layers(const StyleDocument& doc,
                                             double zoom,
                                             const std::string& source_layer) {
  std::vector<const StyleLayer*> out;
  for (const auto& layer : doc.layers) {
    if (!layer_matches_zoom(layer, zoom)) {
      continue;
    }
    // Non-empty request: require exact source-layer (blank layer field does not
    // match).
    if (!source_layer.empty() && layer.source_layer != source_layer) {
      continue;
    }
    out.push_back(&layer);
  }
  return out;
}

bool resolve(const StyleDocument& doc, const SymbolLibrary* library,
             const AttrMap& attrs, double zoom, const std::string& source_layer,
             ResolvedPaint* out) {
  if (!out) {
    return false;
  }
  for (const StyleLayer* layer : select_layers(doc, zoom, source_layer)) {
    if (!eval_filter(layer->filter, attrs)) {
      continue;
    }
    fill_resolved_paint(*layer, library, attrs, zoom, out);
    return true;
  }
  return false;
}

}  // namespace style
}  // namespace gis
