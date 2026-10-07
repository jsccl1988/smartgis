// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/view/local_tool_router.h"

namespace content {

LocalToolRouter::LocalToolRouter() = default;

LocalToolRouter::~LocalToolRouter() = default;

ToolSession* LocalToolRouter::tool_session(uint32_t view_id) {
  auto it = tool_sessions_.find(view_id);
  if (it != tool_sessions_.end()) {
    return it->second.get();
  }
  auto host = std::make_unique<ToolSession>();
  ToolSession* raw = host.get();
  tool_sessions_[view_id] = std::move(host);
  return raw;
}

void LocalToolRouter::close(uint32_t view_id) {
  tool_sessions_.erase(view_id);
}

void LocalToolRouter::bind_edits(uint32_t view_id, gis::EditSession* edits) {
  tool_sessions_[view_id] = std::make_unique<ToolSession>(edits);
}

void LocalToolRouter::set_activate_ipc(ActivateIpc fn) {
  activate_ipc_ = std::move(fn);
}

void LocalToolRouter::set_dispatch_ipc(DispatchIpc fn) {
  dispatch_ipc_ = std::move(fn);
}

void LocalToolRouter::activate(uint32_t view_id, const char* tool_id) {
  ToolSession* h = tool_session(view_id);
  if (tool_id && tool_id[0] != '\0') {
    if (!h->execute(tool_id, view_id)) {
      h->activate(tool_id);
    }
  }
  if (activate_ipc_) {
    activate_ipc_(view_id, tool_id ? tool_id : "");
  }
}

void LocalToolRouter::dispatch(uint32_t view_id, const InputEvent& e) {
  tool_session(view_id)->dispatch_input(e);
  if (dispatch_ipc_) {
    dispatch_ipc_(view_id, e);
  }
}

}  // namespace content
