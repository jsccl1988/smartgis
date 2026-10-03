// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_ASK_H_
#define CONTENT_BROWSER_DEBUG_AGENT_ASK_H_

#include <string>

#include "content/browser/debug/debug_agent.h"

namespace content {
namespace detail {

// Local Ask stub: keyword → existing Agent tools. No remote LLM backend.
// Returns false when |line| is not an :ask command.
bool exec_ask_command(const std::string& line,
                      const DebugAgentHost& host,
                      DebugAgent* agent,
                      std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_ASK_H_
