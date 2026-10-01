// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_UI_H_
#define CONTENT_BROWSER_DEBUG_AGENT_UI_H_

#include <string>

#include "content/browser/debug/debug_agent.h"

namespace content {
namespace detail {

// Handles ui.* and script.run RPC against a host snapshot.
bool dispatch_ui_method(const std::string& method,
                        const std::string& params_json,
                        int id,
                        const DebugAgentHost& host,
                        std::string* response);

// Handles :ui … console commands.
bool exec_ui_command(const std::string& line,
                     const DebugAgentHost& host,
                     std::string* output);

// Handles :script <path>.
bool exec_script_command(const std::string& line,
                         const DebugAgentHost& host,
                         std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_UI_H_
