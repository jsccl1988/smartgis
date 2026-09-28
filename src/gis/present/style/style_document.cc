// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/present/style/style_document.h"

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace style {
namespace {

using Allocator = rapidjson::Document::AllocatorType;

const rapidjson::Value* member(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) {
    return nullptr;
  }
  auto it = obj.FindMember(key);
  return it == obj.MemberEnd() ? nullptr : &it->value;
}

std::string write_json(const rapidjson::Value& v) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  v.Accept(writer);
  return std::string(buf.GetString(), buf.GetSize());
}

std::string value_as_string(const rapidjson::Value& v) {
  if (v.IsString()) {
    return std::string(v.GetString(), v.GetStringLength());
  }
  if (v.IsNumber()) {
    std::ostringstream os;
    os << v.GetDouble();
    return os.str();
  }
  if (v.IsBool()) {
    return v.GetBool() ? "true" : "false";
  }
  if (v.IsNull()) {
    return "";
  }
  return write_json(v);
}

rapidjson::Value make_string(const std::string& s, Allocator& alloc) {
  rapidjson::Value v;
  v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
  return v;
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

bool parse_filter(const rapidjson::Value& v, FilterNode* out) {
  if (!v.IsArray() || v.Empty() || !v[0].IsString()) {
    *out = FilterNode();
    return true;
  }
  out->op = filter_op_from_string(
      std::string(v[0].GetString(), v[0].GetStringLength()));
  out->key.clear();
  out->value.clear();
  out->values.clear();
  out->children.clear();

  switch (out->op) {
    case FilterOp::kAll:
    case FilterOp::kAny:
    case FilterOp::kNone:
      for (rapidjson::SizeType i = 1; i < v.Size(); ++i) {
        FilterNode child;
        if (!parse_filter(v[i], &child)) {
          return false;
        }
        out->children.push_back(std::move(child));
      }
      return true;
    case FilterOp::kHas:
    case FilterOp::kNotHas:
      if (v.Size() < 2 || !v[1].IsString()) {
        return false;
      }
      out->key = std::string(v[1].GetString(), v[1].GetStringLength());
      return true;
    case FilterOp::kIn:
    case FilterOp::kNotIn:
      if (v.Size() < 2 || !v[1].IsString()) {
        return false;
      }
      out->key = std::string(v[1].GetString(), v[1].GetStringLength());
      for (rapidjson::SizeType i = 2; i < v.Size(); ++i) {
        out->values.push_back(value_as_string(v[i]));
      }
      return true;
    case FilterOp::kEq:
    case FilterOp::kNeq:
    case FilterOp::kLt:
    case FilterOp::kLte:
    case FilterOp::kGt:
    case FilterOp::kGte:
      if (v.Size() < 3 || !v[1].IsString()) {
        return false;
      }
      out->key = std::string(v[1].GetString(), v[1].GetStringLength());
      out->value = value_as_string(v[2]);
      return true;
    default:
      *out = FilterNode();
      return true;
  }
}

rapidjson::Value filter_to_json(const FilterNode& f, Allocator& alloc) {
  rapidjson::Value arr(rapidjson::kArrayType);
  if (f.op == FilterOp::kTrue) {
    return arr;
  }
  arr.PushBack(make_string(filter_op_to_string(f.op), alloc), alloc);

  auto push_str = [&](const std::string& s) {
    arr.PushBack(make_string(s, alloc), alloc);
  };

  switch (f.op) {
    case FilterOp::kAll:
    case FilterOp::kAny:
    case FilterOp::kNone:
      for (const auto& c : f.children) {
        arr.PushBack(filter_to_json(c, alloc), alloc);
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

void parse_string_map(const rapidjson::Value* obj,
                      std::map<std::string, std::string>* out) {
  out->clear();
  if (!obj || !obj->IsObject()) {
    return;
  }
  for (auto it = obj->MemberBegin(); it != obj->MemberEnd(); ++it) {
    (*out)[std::string(it->name.GetString(), it->name.GetStringLength())] =
        value_as_string(it->value);
  }
}

bool parse_layer(const rapidjson::Value& v, StyleLayer* out) {
  if (!v.IsObject()) {
    return false;
  }
  *out = StyleLayer();
  if (const rapidjson::Value* id = member(v, "id")) {
    out->id = value_as_string(*id);
  }
  if (const rapidjson::Value* type = member(v, "type")) {
    out->type = layer_type_from_string(value_as_string(*type));
  }
  if (const rapidjson::Value* source = member(v, "source")) {
    out->source = value_as_string(*source);
  }
  if (const rapidjson::Value* sl = member(v, "source-layer")) {
    out->source_layer = value_as_string(*sl);
  }
  if (const rapidjson::Value* mz = member(v, "minzoom"); mz && mz->IsNumber()) {
    out->minzoom = mz->GetDouble();
    out->has_minzoom = true;
  }
  if (const rapidjson::Value* xz = member(v, "maxzoom"); xz && xz->IsNumber()) {
    out->maxzoom = xz->GetDouble();
    out->has_maxzoom = true;
  }
  if (const rapidjson::Value* filter = member(v, "filter")) {
    if (!parse_filter(*filter, &out->filter)) {
      return false;
    }
  }
  parse_string_map(member(v, "paint"), &out->paint);
  parse_string_map(member(v, "layout"), &out->layout);
  return !out->id.empty() && out->type != LayerType::kUnknown;
}

rapidjson::Value string_map_to_json(const std::map<std::string, std::string>& m,
                                    Allocator& alloc) {
  rapidjson::Value obj(rapidjson::kObjectType);
  for (const auto& kv : m) {
    rapidjson::Value key;
    key.SetString(kv.first.c_str(),
                  static_cast<rapidjson::SizeType>(kv.first.size()), alloc);
    obj.AddMember(key, make_string(kv.second, alloc), alloc);
  }
  return obj;
}

rapidjson::Value layer_to_json(const StyleLayer& layer, Allocator& alloc) {
  rapidjson::Value obj(rapidjson::kObjectType);
  auto put_str = [&](const char* key, const std::string& s) {
    if (s.empty()) {
      return;
    }
    rapidjson::Value k;
    k.SetString(key, alloc);
    obj.AddMember(k, make_string(s, alloc), alloc);
  };
  put_str("id", layer.id);
  put_str("type", layer_type_to_string(layer.type));
  put_str("source", layer.source);
  put_str("source-layer", layer.source_layer);
  if (layer.has_minzoom) {
    rapidjson::Value k;
    k.SetString("minzoom", alloc);
    obj.AddMember(k, layer.minzoom, alloc);
  }
  if (layer.has_maxzoom) {
    rapidjson::Value k;
    k.SetString("maxzoom", alloc);
    obj.AddMember(k, layer.maxzoom, alloc);
  }
  if (layer.filter.op != FilterOp::kTrue) {
    rapidjson::Value k;
    k.SetString("filter", alloc);
    obj.AddMember(k, filter_to_json(layer.filter, alloc), alloc);
  }
  if (!layer.paint.empty()) {
    rapidjson::Value k;
    k.SetString("paint", alloc);
    obj.AddMember(k, string_map_to_json(layer.paint, alloc), alloc);
  }
  if (!layer.layout.empty()) {
    rapidjson::Value k;
    k.SetString("layout", alloc);
    obj.AddMember(k, string_map_to_json(layer.layout, alloc), alloc);
  }
  return obj;
}

}  // namespace

bool parse_style_document(const char* json, size_t len, StyleDocument* out) {
  if (!out || !json || len == 0) {
    return false;
  }
  rapidjson::Document root;
  root.Parse(json, static_cast<rapidjson::SizeType>(len));
  if (root.HasParseError() || !root.IsObject()) {
    return false;
  }
  *out = StyleDocument();
  if (const rapidjson::Value* ver = member(root, "version");
      ver && ver->IsNumber()) {
    out->version = static_cast<int>(ver->GetDouble());
  } else {
    return false;
  }
  if (const rapidjson::Value* name = member(root, "name")) {
    out->name = value_as_string(*name);
  }
  if (const rapidjson::Value* sprite = member(root, "sprite")) {
    out->sprite = value_as_string(*sprite);
  }
  if (const rapidjson::Value* glyphs = member(root, "glyphs")) {
    out->glyphs = value_as_string(*glyphs);
  }
  const rapidjson::Value* layers = member(root, "layers");
  if (!layers || !layers->IsArray()) {
    return false;
  }
  for (const auto& layer_json : layers->GetArray()) {
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
  rapidjson::Document root;
  root.SetObject();
  Allocator& alloc = root.GetAllocator();

  {
    rapidjson::Value k;
    k.SetString("version", alloc);
    root.AddMember(k, doc.version, alloc);
  }
  if (!doc.name.empty()) {
    rapidjson::Value k;
    k.SetString("name", alloc);
    root.AddMember(k, make_string(doc.name, alloc), alloc);
  }
  if (!doc.sprite.empty()) {
    rapidjson::Value k;
    k.SetString("sprite", alloc);
    root.AddMember(k, make_string(doc.sprite, alloc), alloc);
  }
  if (!doc.glyphs.empty()) {
    rapidjson::Value k;
    k.SetString("glyphs", alloc);
    root.AddMember(k, make_string(doc.glyphs, alloc), alloc);
  }
  rapidjson::Value layers(rapidjson::kArrayType);
  for (const auto& layer : doc.layers) {
    layers.PushBack(layer_to_json(layer, alloc), alloc);
  }
  {
    rapidjson::Value k;
    k.SetString("layers", alloc);
    root.AddMember(k, layers, alloc);
  }
  return write_json(root);
}

}  // namespace style
}  // namespace gis
