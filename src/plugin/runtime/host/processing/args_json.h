// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PROCESSING_ARGS_JSON_H_
#define PLUGIN_RUNTIME_HOST_PROCESSING_ARGS_JSON_H_

#include <string>
#include <string_view>

#include <rapidjson/document.h>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Shared JSON helpers for product processing / command args (IL + host).
// Empty input becomes `{}` so optional-arg ops stay callable from Interact.
PLUGIN_HOST_EXPORT bool parse_args_json(std::string_view json,
                                        rapidjson::Document* out);
PLUGIN_HOST_EXPORT bool args_json_string(const rapidjson::Value& obj,
                                         const char* key, std::string* out);
PLUGIN_HOST_EXPORT bool args_json_double(const rapidjson::Value& obj,
                                         const char* key, double* out);
PLUGIN_HOST_EXPORT bool args_json_bool(const rapidjson::Value& obj,
                                       const char* key, bool* out);
PLUGIN_HOST_EXPORT bool args_json_int(const rapidjson::Value& obj,
                                      const char* key, int* out);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_PROCESSING_ARGS_JSON_H_
