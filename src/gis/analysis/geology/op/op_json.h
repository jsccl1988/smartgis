// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_OP_OP_JSON_H_
#define GIS_ANALYSIS_GEOLOGY_OP_OP_JSON_H_

#include <string>
#include <string_view>

#include <rapidjson/document.h>

namespace gis {
namespace detail {

bool parse_op_args(std::string_view json, rapidjson::Document* out);

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out);

bool write_text_file(std::string_view path, std::string_view text);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_OP_OP_JSON_H_
