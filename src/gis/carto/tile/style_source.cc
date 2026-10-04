// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/carto/tile/style_source.h"

#include <memory>

// Windows.h may already have defined min/max; RapidJSON needs the names free.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <rapidjson/document.h>

#include "gis/carto/tile/tile_map_layer.h"

namespace gis {
namespace tile {
namespace {

const rapidjson::Value* member(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) {
    return nullptr;
  }
  auto it = obj.FindMember(key);
  return it == obj.MemberEnd() ? nullptr : &it->value;
}

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
                                   const rapidjson::Value& obj,
                                   StyleSourceDesc* out) {
  if (!out) {
    return StyleSourceStatus::kNotObject;
  }
  *out = StyleSourceDesc{};
  out->id = id;
  if (id.empty()) {
    return StyleSourceStatus::kMissingId;
  }
  if (!obj.IsObject()) {
    return StyleSourceStatus::kNotObject;
  }

  const rapidjson::Value* type_v = member(obj, "type");
  if (!type_v || !type_v->IsString()) {
    return StyleSourceStatus::kMissingType;
  }
  out->type = type_from_string(
      std::string(type_v->GetString(), type_v->GetStringLength()));

  if (const rapidjson::Value* ts = member(obj, "tileSize")) {
    if (ts->IsNumber()) {
      out->tile_size = static_cast<int>(ts->GetDouble());
    }
  }
  if (const rapidjson::Value* mz = member(obj, "minzoom")) {
    if (mz->IsNumber()) {
      out->minzoom = static_cast<int>(mz->GetDouble());
      out->has_minzoom = true;
    }
  }
  if (const rapidjson::Value* mz = member(obj, "maxzoom")) {
    if (mz->IsNumber()) {
      out->maxzoom = static_cast<int>(mz->GetDouble());
      out->has_maxzoom = true;
    }
  }

  if (const rapidjson::Value* tiles = member(obj, "tiles")) {
    if (tiles->IsArray()) {
      for (const auto& t : tiles->GetArray()) {
        if (t.IsString() && t.GetStringLength() > 0) {
          out->tiles.emplace_back(t.GetString(), t.GetStringLength());
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

const rapidjson::Value* find_sources_object(const rapidjson::Value& root) {
  if (!root.IsObject()) {
    return nullptr;
  }
  if (const rapidjson::Value* sources = member(root, "sources")) {
    if (sources->IsObject()) {
      return sources;
    }
    return nullptr;
  }
  // Bare sources map: every value is an object with a "type" field.
  bool looks_like_sources = root.MemberCount() > 0;
  for (auto it = root.MemberBegin(); it != root.MemberEnd(); ++it) {
    if (!it->value.IsObject() || !member(it->value, "type")) {
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

StyleSourceStatus parse_style_source(const std::string& id, const char* json,
                                     size_t len, StyleSourceDesc* out) {
  if (!json || !out || len == 0) {
    return StyleSourceStatus::kInvalidJson;
  }
  rapidjson::Document root;
  root.Parse(json, static_cast<rapidjson::SizeType>(len));
  if (root.HasParseError()) {
    return StyleSourceStatus::kInvalidJson;
  }
  return fill_from_object(id, root, out);
}

StyleSourceStatus parse_style_source(const std::string& id,
                                     const std::string& json,
                                     StyleSourceDesc* out) {
  return parse_style_source(id, json.data(), json.size(), out);
}

StyleSourceStatus parse_style_sources(const char* json, size_t len,
                                      std::vector<StyleSourceDesc>* out) {
  if (!json || !out || len == 0) {
    return StyleSourceStatus::kInvalidJson;
  }
  out->clear();
  rapidjson::Document root;
  root.Parse(json, static_cast<rapidjson::SizeType>(len));
  if (root.HasParseError()) {
    return StyleSourceStatus::kInvalidJson;
  }
  const rapidjson::Value* sources = find_sources_object(root);
  if (!sources) {
    return StyleSourceStatus::kNotObject;
  }

  StyleSourceStatus aggregate = StyleSourceStatus::kOk;
  for (auto it = sources->MemberBegin(); it != sources->MemberEnd(); ++it) {
    StyleSourceDesc desc;
    const std::string sid(it->name.GetString(), it->name.GetStringLength());
    const StyleSourceStatus st = fill_from_object(sid, it->value, &desc);
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
}  // namespace gis
