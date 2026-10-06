// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/detail/json.h"

namespace plugin {
namespace detail {

bool parse_scene_args(std::string_view json, rapidjson::Document* out) {
  if (!out) {
    return false;
  }
  if (json.empty()) {
    out->SetObject();
    return true;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool scene_json_get_string(const rapidjson::Value& obj, const char* key,
                           std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return true;
}

bool scene_json_get_double(const rapidjson::Value& obj, const char* key,
                           double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool scene_json_get_bool(const rapidjson::Value& obj, const char* key,
                         bool* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsBool()) {
    return false;
  }
  *out = it->value.GetBool();
  return true;
}

}  // namespace detail
}  // namespace plugin
