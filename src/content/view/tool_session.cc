// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/tool_session.h"

#include "content/public/event_bus.h"
#include "gis/edit/memory_session.h"
#include "tool/msg/msg.h"
#include "tool/workspace/workspace.h"

namespace content {

struct ToolSession::Impl {
  EventBus events;
  std::unique_ptr<gis::MemoryEditSession> owned;
  gis::EditSession* edits = nullptr;
  std::unique_ptr<tool::Workspace> workspace;
};

ToolSession::ToolSession() : ToolSession(nullptr) {}

ToolSession::ToolSession(gis::EditSession* edits) : impl_(std::make_unique<Impl>()) {
  if (edits) {
    impl_->edits = edits;
  } else {
    impl_->owned = std::make_unique<gis::MemoryEditSession>();
    impl_->edits = impl_->owned.get();
  }
  impl_->workspace =
      std::make_unique<tool::Workspace>(&impl_->events, impl_->edits);
}

ToolSession::~ToolSession() = default;

EventBus* ToolSession::events() {
  return impl_ ? &impl_->events : nullptr;
}

gis::EditSession* ToolSession::edits() {
  return impl_ ? impl_->edits : nullptr;
}

tool::Workspace* ToolSession::workspace() {
  return impl_ ? impl_->workspace.get() : nullptr;
}

bool ToolSession::execute(std::string_view command_id, uint32_t view_id) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  tool::CommandArgs args;
  args.view_id = view_id;
  return impl_->workspace->execute(command_id, args);
}

bool ToolSession::activate(std::string_view interaction_id) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  return impl_->workspace->activate(interaction_id);
}

bool ToolSession::dispatch_input(const InputEvent& e) {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  return impl_->workspace->dispatch_input(e);
}

bool ToolSession::execute_legacy(long gt_msg) {
  return tool::try_execute_gt_msg(workspace(), gt_msg);
}

void ToolSession::release_exclusive() {
  if (!impl_ || !impl_->workspace) {
    return;
  }
  while (impl_->workspace->stack().pop()) {
  }
}

bool ToolSession::flashing() const {
  if (!impl_ || !impl_->workspace) {
    return false;
  }
  const auto addr = reinterpret_cast<uintptr_t>(impl_->workspace.get());
  if (addr < 0x10000ull || (addr >> 48) != 0) {
    return false;
  }
  return impl_->workspace->flashing();
}

}  // namespace content
