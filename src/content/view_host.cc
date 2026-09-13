// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/view_host.h"

#include "content/public/event_bus.h"
#include "sdb/edit/edit_session.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

namespace content {

struct ViewHost::Impl {
  EventBus events;
  std::unique_ptr<sdb::MemoryEditSession> owned;
  sdb::EditSession* edits = nullptr;
  std::unique_ptr<tool::Workspace> workspace;
};

ViewHost::ViewHost() : ViewHost(nullptr) {}

ViewHost::ViewHost(sdb::EditSession* edits) : impl_(std::make_unique<Impl>()) {
  if (edits) {
    impl_->edits = edits;
  } else {
    impl_->owned = std::make_unique<sdb::MemoryEditSession>();
    impl_->edits = impl_->owned.get();
  }
  impl_->workspace =
      std::make_unique<tool::Workspace>(&impl_->events, impl_->edits);
}

ViewHost::~ViewHost() = default;

EventBus* ViewHost::events() {
  return impl_ ? &impl_->events : nullptr;
}

sdb::EditSession* ViewHost::edits() {
  return impl_ ? impl_->edits : nullptr;
}

tool::Workspace* ViewHost::workspace() {
  return impl_ ? impl_->workspace.get() : nullptr;
}

bool ViewHost::execute(std::string_view command_id, uint32_t view_id) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  tool::CommandArgs args;
  args.view_id = view_id;
  return impl_->workspace->execute(command_id, args);
}

bool ViewHost::activate(std::string_view interaction_id) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  return impl_->workspace->activate(interaction_id);
}

bool ViewHost::dispatch_input(const InputEvent& e) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  return impl_->workspace->dispatch_input(e);
}

bool ViewHost::execute_legacy(long gt_msg) {
  const char* id = tool::command_id_from_gt_msg(gt_msg);
  if (!id) {
    return false;
  }
  execute(id);
  return true;
}

void ViewHost::release_exclusive() {
  if (!impl_ || !impl_->workspace) {
    return;
  }
  while (impl_->workspace->stack().pop()) {
  }
}

bool ViewHost::flashing() const {
  return impl_ && impl_->workspace && impl_->workspace->flashing();
}

}  // namespace content
