// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/agent_map_cmd.h"

#include <sstream>

namespace content {
namespace detail {

bool exec_map_command(const std::string& line,
                      const DebugAgentHost& host,
                      std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":refresh") {
    if (host.refresh_map) {
      host.refresh_map();
      *output = "refreshed";
      return true;
    }
    *output = "no host.refresh_map";
    return true;
  }
  if (line == ":extent") {
    if (host.extent_string) {
      *output = host.extent_string();
      return true;
    }
    *output = "no host.extent_string";
    return true;
  }
  if (line == ":layers") {
    if (!host.layer_names) {
      *output = "no host.layer_names";
      return true;
    }
    std::ostringstream oss;
    const auto names = host.layer_names();
    for (size_t i = 0; i < names.size(); ++i) {
      if (i) {
        oss << '\n';
      }
      oss << names[i];
    }
    *output = oss.str();
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
