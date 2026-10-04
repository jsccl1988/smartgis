// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_SDBD_H_
#define CONTENT_BROWSER_DEBUG_AGENT_SDBD_H_

#include <string>

namespace content {
namespace detail {

// Handles sdbd.capabilities / collections / query RPC.
bool dispatch_sdbd_method(const std::string& method,
                          const std::string& params_json,
                          int id,
                          std::string* response);

// Handles :sdbd capabilities|sql console commands.
bool exec_sdbd_command(const std::string& line, std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_SDBD_H_
