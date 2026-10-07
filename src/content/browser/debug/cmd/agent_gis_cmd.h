// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_MAP_CMD_H_
#define CONTENT_BROWSER_DEBUG_AGENT_MAP_CMD_H_

#include <string>

#include "content/browser/debug/debug_agent.h"

namespace content {
namespace detail {

// Handles :refresh / :extent / :layers against a host snapshot.
bool exec_gis_command(const std::string& line,
                      const DebugAgentHost& host,
                      std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_MAP_CMD_H_
