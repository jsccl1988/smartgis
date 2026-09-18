// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/style_source.h"

#include "sdb/style/json_mini.h"
#include "sdb/tile/tile_map_layer.h"

#include <memory>

namespace sdb {
namespace tile {
namespace {

using style::detail::JsonKind;
using style::detail::JsonValue;

StyleSourceType type_from_string(const std::string& s) {
  if (s == "raster") {
    return StyleSourceType::kRaster;
  }
  if (s == "vector") {
    return StyleSourceType::kVector;
  }
  if (s.empty()) {
    return StyleSourceType::kUnknown;
  }
  return StyleSourceType::kUnsupported;
}

bool has_xyz_placeholders(const std::string& tmpl) {
  return tmpl.find("{z}") != std::string::npos &&
         tmpl.find("{x}") != std::string::npos &&
         tmpl.find("{y}") != std::string::npos;
}

StyleSourceStatus fill_from_object(const std::string& id,
                                   const JsonValue& obj,
                                   StyleSourceDesc* out) {
  if (!out) {
    return StyleSourceStatus::kNotObject;
  }
  *out = StyleSourceDesc{};
  out->id = id;
  if (id.empty()) {
    return StyleSourceStatus::kMissingId;
  }
  if (!obj.is_object()) {
    return StyleSourceStatus::kNotObject;
  }

  const JsonValue* type_v = obj.get("type");
  if (!type_v || !type_v->is_string()) {
    return StyleSourceStatus::kMissingType;
  }
  out->type = type_from_string(type_v->s);

  if (const JsonValue* ts = obj.get("tileSize")) {
    if (ts->is_number()) {
      out->tile_size = static_cast<int>(ts->n);
    }
  }
  if (const JsonValue* mz = obj.get("minzoom")) {
    if (mz->is_number()) {
      out->minzoom = static_cast<int>(mz->n);
      out->has_minzoom = true;
    }
  }
  if (const JsonValue* mz = obj.get("maxzoom")) {
    if (mz->is_number()) {
      out->maxzoom = static_cast<int>(mz->n);
      out->has_maxzoom = true;
    }
  }

  if (const JsonValue* tiles = obj.get("tiles")) {
    if (tiles->is_array()) {
      for (const JsonValue& t : tiles->a) {
        if (t.is_string() && !t.s.empty()) {
          out->tiles.push_back(t.s);
        }
      }
    }
  }

  if (out->type == StyleSourceType::kVector) {
    return StyleSourceStatus::kVectorUnsupported;
  }
  if (out->type == StyleSourceType::kUnsupported) {
    return StyleSourceStatus::kUnsupportedType;
  }
  if (out->type == StyleSourceType::kUnknown) {
    return StyleSourceStatus::kMissingType;
  }

  // Raster path.
  if (out->tiles.empty()) {
    return StyleSourceStatus::kMissingTiles;
  }
  if (out->primary_url_template().empty()) {
    return StyleSourceStatus::kEmptyTiles;
  }
  if (!has_xyz_placeholders(out->primary_url_template())) {
    return StyleSourceStatus::kBadUrlTemplate;
  }
  return StyleSourceStatus::kOk;
}

const JsonValue* find_sources_object(const JsonValue& root) {
  if (!root.is_object()) {
    return nullptr;
  }
  if (const JsonValue* sources = root.get("sources")) {
    if (sources->is_object()) {
      return sources;
    }
    return nullptr;
  }
  // Bare sources map: every value is an object with a "type" field.
  bool looks_like_sources = !root.o.empty();
  for (const auto& kv : root.o) {
    if (!kv.second.is_object() || !kv.second.get("type")) {
      looks_like_sources = false;
      break;
    }
  }
  return looks_like_sources ? &root : nullptr;
}

}  // namespace

const char* style_source_type_name(StyleSourceType t) {
  switch (t) {
    case StyleSourceType::kRaster:
      return "raster";
    case StyleSourceType::kVector:
      return "vector";
    case StyleSourceType::kUnsupported:
      return "unsupported";
    case StyleSourceType::kUnknown:
    default:
      return "unknown";
  }
}

const char* style_source_status_name(StyleSourceStatus s) {
  switch (s) {
    case StyleSourceStatus::kOk:
      return "ok";
    case StyleSourceStatus::kInvalidJson:
      return "invalid_json";
    case StyleSourceStatus::kNotObject:
      return "not_object";
    case StyleSourceStatus::kMissingId:
      return "missing_id";
    case StyleSourceStatus::kMissingType:
      return "missing_type";
    case StyleSourceStatus::kMissingTiles:
      return "missing_tiles";
    case StyleSourceStatus::kEmptyTiles:
      return "empty_tiles";
    case StyleSourceStatus::kBadUrlTemplate:
      return "bad_url_template";
    case StyleSourceStatus::kVectorUnsupported:
      return "vector_unsupported";
    case StyleSourceStatus::kUnsupportedType:
      return "unsupported_type";
    default:
      return "unknown";
  }
}

StyleSourceStatus parse_style_source(const std::string& id,
                                     const char* json,
                                     size_t len,
                                     StyleSourceDesc* out) {
  if (!json || !out) {
    return StyleSourceStatus::kInvalidJson;
  }
  JsonValue root;
  if (!style::detail::parse_json(json, len, &root)) {
    return StyleSourceStatus::kInvalidJson;
  }
  return fill_from_object(id, root, out);
}

StyleSourceStatus parse_style_source(const std::string& id,
                                     const std::string& json,
                                     StyleSourceDesc* out) {
  return parse_style_source(id, json.data(), json.size(), out);
}

StyleSourceStatus parse_style_sources(const char* json,
                                      size_t len,
                                      std::vector<StyleSourceDesc>* out) {
  if (!json || !out) {
    return StyleSourceStatus::kInvalidJson;
  }
  out->clear();
  JsonValue root;
  if (!style::detail::parse_json(json, len, &root)) {
    return StyleSourceStatus::kInvalidJson;
  }
  const JsonValue* sources = find_sources_object(root);
  if (!sources) {
    return StyleSourceStatus::kNotObject;
  }

  StyleSourceStatus aggregate = StyleSourceStatus::kOk;
  for (const auto& kv : sources->o) {
    StyleSourceDesc desc;
    const StyleSourceStatus st = fill_from_object(kv.first, kv.second, &desc);
    if (st == StyleSourceStatus::kOk) {
      out->push_back(std::move(desc));
      continue;
    }
    // Prefer reporting vector over other failures when mixed.
    if (st == StyleSourceStatus::kVectorUnsupported) {
      aggregate = StyleSourceStatus::kVectorUnsupported;
      continue;
    }
    if (aggregate == StyleSourceStatus::kOk) {
      aggregate = st;
    }
  }
  return aggregate;
}

StyleSourceStatus parse_style_sources(const std::string& json,
                                      std::vector<StyleSourceDesc>* out) {
  return parse_style_sources(json.data(), json.size(), out);
}

bool is_raster_bindable(const StyleSourceDesc& desc) {
  return desc.type == StyleSourceType::kRaster &&
         !desc.primary_url_template().empty() &&
         has_xyz_placeholders(desc.primary_url_template());
}

bool open_provider_from_source(const StyleSourceDesc& desc,
                               TileProvider* provider) {
  if (!provider || !is_raster_bindable(desc)) {
    return false;
  }
  return provider->open_xyz(desc.primary_url_template());
}

MapLayer make_xyz_map_layer_from_source(const StyleSourceDesc& desc) {
  if (!is_raster_bindable(desc)) {
    return MapLayer{};
  }
  return make_xyz_map_layer(desc.primary_url_template());
}

}  // namespace tile
}  // namespace sdb
