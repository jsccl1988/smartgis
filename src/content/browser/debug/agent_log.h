// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_LOG_H_
#define CONTENT_BROWSER_DEBUG_AGENT_LOG_H_

#include <string>

#include "base/log/log_sink.h"

namespace content {
namespace detail {

std::string log_entry_json(const base::LogEntry& e);

// Handles log.tail / log.set_level. Returns true when |method| is owned here.
bool dispatch_log_method(const std::string& method,
                         const std::string& params_json,
                         int id,
                         std::string* response);

// Handles :clear / :log.level. Returns true when |line| is owned here.
bool exec_log_command(const std::string& line, std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_LOG_H_
