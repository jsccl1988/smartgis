// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/local_tool_router.h"

namespace content {

LocalToolRouter::LocalToolRouter() = default;

LocalToolRouter::~LocalToolRouter() = default;

ViewHost* LocalToolRouter::host(uint32_t view_id) {
  auto it = hosts_.find(view_id);
  if (it != hosts_.end()) {
    return it->second.get();
  }
  auto host = std::make_unique<ViewHost>();
  ViewHost* raw = host.get();
  hosts_[view_id] = std::move(host);
  return raw;
}

void LocalToolRouter::close(uint32_t view_id) {
  hosts_.erase(view_id);
}

void LocalToolRouter::bind_edits(uint32_t view_id, sdb::EditSession* edits) {
  hosts_[view_id] = std::make_unique<ViewHost>(edits);
}

void LocalToolRouter::set_activate_ipc(ActivateIpc fn) {
  activate_ipc_ = std::move(fn);
}

void LocalToolRouter::set_dispatch_ipc(DispatchIpc fn) {
  dispatch_ipc_ = std::move(fn);
}

void LocalToolRouter::activate(uint32_t view_id, const char* tool_id) {
  ViewHost* h = host(view_id);
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
  host(view_id)->dispatch_input(e);
  if (dispatch_ipc_) {
    dispatch_ipc_(view_id, e);
  }
}

}  // namespace content
