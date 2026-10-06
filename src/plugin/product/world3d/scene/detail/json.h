// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_DETAIL_JSON_H_
#define PLUGIN_WORLD3D_SCENE_DETAIL_JSON_H_

#include <string>
#include <string_view>

#include <rapidjson/document.h>

namespace plugin {
namespace detail {

bool parse_scene_args(std::string_view json, rapidjson::Document* out);
bool scene_json_get_string(const rapidjson::Value& obj, const char* key,
                           std::string* out);
bool scene_json_get_double(const rapidjson::Value& obj, const char* key,
                           double* out);
bool scene_json_get_bool(const rapidjson::Value& obj, const char* key,
                         bool* out);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_DETAIL_JSON_H_
