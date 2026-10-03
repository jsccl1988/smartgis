// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_DIAG_H_
#define CONTENT_BROWSER_DEBUG_AGENT_DIAG_H_

#include <string>

#include "content/browser/debug/debug_agent.h"

namespace content {
namespace detail {

// One-shot diagnostic pack: log tail + extent + layers + UI tree + optional
// shell capture. Returns a JSON object string (not wrapped in ok_result).
std::string build_diag_pack(const DebugAgentHost& host,
                            int log_n,
                            bool capture);

bool dispatch_diag_method(const std::string& method,
                          const std::string& params_json,
                          int id,
                          const DebugAgentHost& host,
                          std::string* response);

bool exec_diag_command(const std::string& line,
                       const DebugAgentHost& host,
                       std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_DIAG_H_
