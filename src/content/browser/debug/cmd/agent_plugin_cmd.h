// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_CMD_AGENT_PLUGIN_CMD_H_
#define CONTENT_BROWSER_DEBUG_CMD_AGENT_PLUGIN_CMD_H_

#include <string>

#include "content/browser/debug/debug_agent.h"

namespace content {
namespace detail {

bool exec_plugin_command(const std::string& line,
                         const DebugAgentHost& host,
                         std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_CMD_AGENT_PLUGIN_CMD_H_
