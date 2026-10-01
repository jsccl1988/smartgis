// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_log.h"

#include <sstream>

#include "content/browser/debug/agent_json.h"

namespace content {
namespace detail {

std::string log_entry_json(const base::LogEntry& e) {
  std::ostringstream oss;
  oss << "{\"level\":\"" << base::log_level_name(e.level) << "\""
      << ",\"timestamp\":\"" << json_escape(e.timestamp) << "\""
      << ",\"tid\":" << e.tid << ",\"file\":\"" << json_escape(e.file) << "\""
      << ",\"line\":" << e.line << ",\"func\":\"" << json_escape(e.func) << "\""
      << ",\"message\":\"" << json_escape(e.message) << "\"}";
  return oss.str();
}

bool dispatch_log_method(const std::string& method,
                         const std::string& params_json,
                         int id,
                         std::string* response) {
  if (!response) {
    return false;
  }
  if (method == "log.tail") {
    int n = 200;
    extract_int_field(params_json, "n", &n);
    if (n < 0) {
      n = 0;
    }
    const auto entries = base::log_sink().snapshot_tail(static_cast<size_t>(n));
    std::ostringstream oss;
    oss << "{\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
      if (i) {
        oss << ',';
      }
      oss << log_entry_json(entries[i]);
    }
    oss << "]}";
    *response = ok_result(id, oss.str());
    return true;
  }
  if (method == "log.set_level") {
    std::string level_name;
    extract_string_field(params_json, "level", &level_name);
    base::LogLevel level = base::LogLevel::kInfo;
    if (!base::parse_log_level(level_name, &level)) {
      *response = err_result(id, "bad level");
      return true;
    }
    base::log_sink().set_min_level(level);
    *response = ok_result(id, "{}");
    return true;
  }
  return false;
}

bool exec_log_command(const std::string& line, std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":clear") {
    base::log_sink().clear();
    *output = "cleared";
    return true;
  }
  if (line.rfind(":log.level ", 0) == 0) {
    base::LogLevel level = base::LogLevel::kInfo;
    if (!base::parse_log_level(line.substr(11), &level)) {
      *output = "bad level";
      return true;
    }
    base::log_sink().set_min_level(level);
    *output = std::string("min_level=") + base::log_level_name(level);
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
