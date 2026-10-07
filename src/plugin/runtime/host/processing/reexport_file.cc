// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/processing/reexport_file.h"

#include <cstdio>
#include <string>

#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"

#include <rapidjson/document.h>

namespace plugin {

bool reexport_cached_file(std::string* cached_path, std::string_view args_json,
                          const char* op_name) {
  if (!cached_path || !op_name) {
    return false;
  }
  std::string dest = *cached_path;
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    std::string out;
    if (args_json_string(args, "output", &out) && !out.empty()) {
      dest = std::move(out);
    }
  }
  if (dest.empty()) {
    set_operation_result(std::string("{\"error\":\"no_output\",\"op\":\"") +
                         op_name + "\"}");
    return false;
  }
  if (!cached_path->empty() && dest != *cached_path) {
    FILE* in = nullptr;
    FILE* out = nullptr;
    if (fopen_s(&in, cached_path->c_str(), "rb") != 0 || !in) {
      set_operation_result(
          std::string("{\"error\":\"missing_source\",\"op\":\"") + op_name +
          "\"}");
      return false;
    }
    if (fopen_s(&out, dest.c_str(), "wb") != 0 || !out) {
      fclose(in);
      set_operation_result(std::string("{\"error\":\"write_failed\",\"op\":\"") +
                           op_name + "\"}");
      return false;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
      if (fwrite(buf, 1, n, out) != n) {
        fclose(in);
        fclose(out);
        set_operation_result(
            std::string("{\"error\":\"write_failed\",\"op\":\"") + op_name +
            "\"}");
        return false;
      }
    }
    fclose(in);
    fclose(out);
    *cached_path = dest;
  } else {
    FILE* f = nullptr;
    if (fopen_s(&f, dest.c_str(), "rb") != 0 || !f) {
      set_operation_result(
          std::string("{\"error\":\"missing_output\",\"op\":\"") + op_name +
          "\"}");
      return false;
    }
    fclose(f);
  }
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op_name +
                       "\",\"output\":\"" + dest + "\"}");
  return true;
}

}  // namespace plugin
