// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// JSON object decode for builtin operator arguments.

#ifndef GIS_ANALYSIS_OPS_OPS_ARGS_H_
#define GIS_ANALYSIS_OPS_OPS_ARGS_H_

#include <string>
#include <string_view>

#include <rapidjson/document.h>

namespace gis {
namespace detail {

bool decode_op_args(std::string_view json, rapidjson::Document* out);

bool op_arg_string(const rapidjson::Value& obj, const char* key, std::string* out);

bool op_arg_double(const rapidjson::Value& obj, const char* key, double* out);

bool op_arg_int(const rapidjson::Value& obj, const char* key, int* out);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_OPS_ARGS_H_
