// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/cmd/agent_plugin_cmd.h"

namespace content {
namespace detail {

bool exec_plugin_command(const std::string& line,
                         const DebugAgentHost& host,
                         std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":plugin sweep") {
    if (!host.plugin_sweep) {
      *output = "no host.plugin_sweep";
      return true;
    }
    *output = host.plugin_sweep();
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
