// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/style/style_document.h"

#include <sstream>

#include "gis/style/json_mini.h"

namespace gis {
namespace style {
namespace {

using detail::JsonKind;
using detail::JsonValue;

std::string value_as_string(const JsonValue& v) {
  switch (v.kind) {
    case JsonKind::kString:
      return v.s;
    case JsonKind::kNumber: {
      std::ostringstream os;
      os << v.n;
      return os.str();
    }
    case JsonKind::kBool:
      return v.b ? "true" : "false";
    case JsonKind::kNull:
      return "";
    default:
      return detail::json_to_string(v);
  }
}

FilterOp filter_op_from_string(const std::string& s) {
  if (s == "==") {
    return FilterOp::kEq;
  }
  if (s == "!=") {
    return FilterOp::kNeq;
  }
  if (s == "<") {
    return FilterOp::kLt;
  }
  if (s == "<=") {
    return FilterOp::kLte;
  }
  if (s == ">") {
    return FilterOp::kGt;
  }
  if (s == ">=") {
    return FilterOp::kGte;
  }
  if (s == "has") {
    return FilterOp::kHas;
  }
  if (s == "!has") {
    return FilterOp::kNotHas;
  }
  if (s == "in") {
    return FilterOp::kIn;
  }
  if (s == "!in") {
    return FilterOp::kNotIn;
  }
  if (s == "all") {
    return FilterOp::kAll;
  }
  if (s == "any") {
    return FilterOp::kAny;
  }
  if (s == "none") {
    return FilterOp::kNone;
  }
  return FilterOp::kTrue;
}

const char* filter_op_to_string(FilterOp op) {
  switch (op) {
    case FilterOp::kEq:
      return "==";
    case FilterOp::kNeq:
      return "!=";
    case FilterOp::kLt:
      return "<";
    case FilterOp::kLte:
      return "<=";
    case FilterOp::kGt:
      return ">";
    case FilterOp::kGte:
      return ">=";
    case FilterOp::kHas:
      return "has";
    case FilterOp::kNotHas:
      return "!has";
    case FilterOp::kIn:
      return "in";
    case FilterOp::kNotIn:
      return "!in";
    case FilterOp::kAll:
      return "all";
    case FilterOp::kAny:
      return "any";
    case FilterOp::kNone:
      return "none";
    default:
      return "all";
  }
}

bool parse_filter(const JsonValue& v, FilterNode* out) {
  if (!v.is_array() || v.a.empty() || !v.a[0].is_string()) {
    *out = FilterNode();
    return true;
  }
  out->op = filter_op_from_string(v.a[0].s);
  out->key.clear();
  out->value.clear();
  out->values.clear();
  out->children.clear();

  switch (out->op) {
    case FilterOp::kAll:
    case FilterOp::kAny:
    case FilterOp::kNone:
      for (size_t i = 1; i < v.a.size(); ++i) {
        FilterNode child;
        if (!parse_filter(v.a[i], &child)) {
          return false;
        }
        out->children.push_back(std::move(child));
      }
      return true;
    case FilterOp::kHas:
    case FilterOp::kNotHas:
      if (v.a.size() < 2 || !v.a[1].is_string()) {
        return false;
      }
      out->key = v.a[1].s;
      return true;
    case FilterOp::kIn:
    case FilterOp::kNotIn:
      if (v.a.size() < 2 || !v.a[1].is_string()) {
        return false;
      }
      out->key = v.a[1].s;
      for (size_t i = 2; i < v.a.size(); ++i) {
        out->values.push_back(value_as_string(v.a[i]));
      }
      return true;
    case FilterOp::kEq:
    case FilterOp::kNeq:
    case FilterOp::kLt:
    case FilterOp::kLte:
    case FilterOp::kGt:
    case FilterOp::kGte:
      if (v.a.size() < 3 || !v.a[1].is_string()) {
        return false;
      }
      out->key = v.a[1].s;
      out->value = value_as_string(v.a[2]);
      return true;
    default:
      *out = FilterNode();
      return true;
  }
}

JsonValue filter_to_json(const FilterNode& f) {
  JsonValue arr;
  arr.kind = JsonKind::kArray;
  if (f.op == FilterOp::kTrue) {
    return arr;
  }
  JsonValue op;
  op.kind = JsonKind::kString;
  op.s = filter_op_to_string(f.op);
  arr.a.push_back(std::move(op));

  auto push_str = [&](const std::string& s) {
    JsonValue j;
    j.kind = JsonKind::kString;
    j.s = s;
    arr.a.push_back(std::move(j));
  };

  switch (f.op) {
    case FilterOp::kAll:
    case FilterOp::kAny:
    case FilterOp::kNone:
      for (const auto& c : f.children) {
        arr.a.push_back(filter_to_json(c));
      }
      break;
    case FilterOp::kHas:
    case FilterOp::kNotHas:
      push_str(f.key);
      break;
    case FilterOp::kIn:
    case FilterOp::kNotIn:
      push_str(f.key);
      for (const auto& val : f.values) {
        push_str(val);
      }
      break;
    default:
      push_str(f.key);
      push_str(f.value);
      break;
  }
  return arr;
}

void parse_string_map(const JsonValue* obj,
                      std::map<std::string, std::string>* out) {
  out->clear();
  if (!obj || !obj->is_object()) {
    return;
  }
  for (const auto& kv : obj->o) {
    (*out)[kv.first] = value_as_string(kv.second);
  }
}

bool parse_layer(const JsonValue& v, StyleLayer* out) {
  if (!v.is_object()) {
    return false;
  }
  *out = StyleLayer();
  if (const JsonValue* id = v.get("id")) {
    out->id = value_as_string(*id);
  }
  if (const JsonValue* type = v.get("type")) {
    out->type = layer_type_from_string(value_as_string(*type));
  }
  if (const JsonValue* source = v.get("source")) {
    out->source = value_as_string(*source);
  }
  if (const JsonValue* sl = v.get("source-layer")) {
    out->source_layer = value_as_string(*sl);
  }
  if (const JsonValue* mz = v.get("minzoom"); mz && mz->is_number()) {
    out->minzoom = mz->n;
    out->has_minzoom = true;
  }
  if (const JsonValue* xz = v.get("maxzoom"); xz && xz->is_number()) {
    out->maxzoom = xz->n;
    out->has_maxzoom = true;
  }
  if (const JsonValue* filter = v.get("filter")) {
    if (!parse_filter(*filter, &out->filter)) {
      return false;
    }
  }
  parse_string_map(v.get("paint"), &out->paint);
  parse_string_map(v.get("layout"), &out->layout);
  return !out->id.empty() && out->type != LayerType::kUnknown;
}

JsonValue string_map_to_json(const std::map<std::string, std::string>& m) {
  JsonValue obj;
  obj.kind = JsonKind::kObject;
  for (const auto& kv : m) {
    JsonValue s;
    s.kind = JsonKind::kString;
    s.s = kv.second;
    obj.o.emplace(kv.first, std::move(s));
  }
  return obj;
}

JsonValue layer_to_json(const StyleLayer& layer) {
  JsonValue obj;
  obj.kind = JsonKind::kObject;
  auto put_str = [&](const char* key, const std::string& s) {
    if (s.empty()) {
      return;
    }
    JsonValue j;
    j.kind = JsonKind::kString;
    j.s = s;
    obj.o.emplace(key, std::move(j));
  };
  put_str("id", layer.id);
  put_str("type", layer_type_to_string(layer.type));
  put_str("source", layer.source);
  put_str("source-layer", layer.source_layer);
  if (layer.has_minzoom) {
    JsonValue n;
    n.kind = JsonKind::kNumber;
    n.n = layer.minzoom;
    obj.o.emplace("minzoom", std::move(n));
  }
  if (layer.has_maxzoom) {
    JsonValue n;
    n.kind = JsonKind::kNumber;
    n.n = layer.maxzoom;
    obj.o.emplace("maxzoom", std::move(n));
  }
  if (layer.filter.op != FilterOp::kTrue) {
    obj.o.emplace("filter", filter_to_json(layer.filter));
  }
  if (!layer.paint.empty()) {
    obj.o.emplace("paint", string_map_to_json(layer.paint));
  }
  if (!layer.layout.empty()) {
    obj.o.emplace("layout", string_map_to_json(layer.layout));
  }
  return obj;
}

}  // namespace

bool parse_style_document(const char* json, size_t len, StyleDocument* out) {
  if (!out) {
    return false;
  }
  JsonValue root;
  if (!detail::parse_json(json, len, &root) || !root.is_object()) {
    return false;
  }
  *out = StyleDocument();
  if (const JsonValue* ver = root.get("version"); ver && ver->is_number()) {
    out->version = static_cast<int>(ver->n);
  } else {
    return false;
  }
  if (const JsonValue* name = root.get("name")) {
    out->name = value_as_string(*name);
  }
  if (const JsonValue* sprite = root.get("sprite")) {
    out->sprite = value_as_string(*sprite);
  }
  if (const JsonValue* glyphs = root.get("glyphs")) {
    out->glyphs = value_as_string(*glyphs);
  }
  const JsonValue* layers = root.get("layers");
  if (!layers || !layers->is_array()) {
    return false;
  }
  for (const auto& layer_json : layers->a) {
    StyleLayer layer;
    if (!parse_layer(layer_json, &layer)) {
      return false;
    }
    out->layers.push_back(std::move(layer));
  }
  return true;
}

bool parse_style_document(const std::string& json, StyleDocument* out) {
  return parse_style_document(json.data(), json.size(), out);
}

std::string serialize_style_document(const StyleDocument& doc) {
  JsonValue root;
  root.kind = JsonKind::kObject;
  JsonValue ver;
  ver.kind = JsonKind::kNumber;
  ver.n = doc.version;
  root.o.emplace("version", std::move(ver));
  if (!doc.name.empty()) {
    JsonValue n;
    n.kind = JsonKind::kString;
    n.s = doc.name;
    root.o.emplace("name", std::move(n));
  }
  if (!doc.sprite.empty()) {
    JsonValue s;
    s.kind = JsonKind::kString;
    s.s = doc.sprite;
    root.o.emplace("sprite", std::move(s));
  }
  if (!doc.glyphs.empty()) {
    JsonValue g;
    g.kind = JsonKind::kString;
    g.s = doc.glyphs;
    root.o.emplace("glyphs", std::move(g));
  }
  JsonValue layers;
  layers.kind = JsonKind::kArray;
  for (const auto& layer : doc.layers) {
    layers.a.push_back(layer_to_json(layer));
  }
  root.o.emplace("layers", std::move(layers));
  return detail::json_to_string(root);
}

}  // namespace style
}  // namespace gis
