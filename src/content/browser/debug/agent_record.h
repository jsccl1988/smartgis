// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_RECORD_H_
#define CONTENT_BROWSER_DEBUG_AGENT_RECORD_H_

#include <functional>
#include <string>

namespace content {
namespace detail {

// Callbacks into DebugAgent record ring buffer state.
struct RecordHandlers {
  std::function<void(bool on)> set_enabled;
  std::function<std::string()> poll_json;
  std::function<void()> clear;
};

// Handles record.enable / poll / clear RPC.
bool dispatch_record_method(const std::string& method,
                            const std::string& params_json,
                            int id,
                            const RecordHandlers& handlers,
                            std::string* response);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_RECORD_H_
