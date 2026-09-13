// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/edit/edit_session.h"

#include "base/core/command.h"

namespace sdb {
namespace {

class MutationReceiver final : public base::SmtCommandReceiver {
 public:
  MutationReceiver(CommandEditSession::ApplyFn apply, FeatureMutation mutation)
      : apply_(std::move(apply)), mutation_(mutation) {}

  bool action(bool bUndo) override {
    if (!apply_) {
      return false;
    }
    return apply_(mutation_, bUndo);
  }

 private:
  CommandEditSession::ApplyFn apply_;
  FeatureMutation mutation_;
};

}  // namespace

struct CommandEditSession::Impl {
  ApplyFn apply;
  base::SmtCommandManager mgr;
};

CommandEditSession::CommandEditSession(ApplyFn apply)
    : impl_(std::make_unique<Impl>()) {
  impl_->apply = std::move(apply);
}

CommandEditSession::~CommandEditSession() {
  if (impl_) {
    impl_->mgr.clear_all_commands();
  }
}

bool CommandEditSession::commit(const FeatureMutation& mutation) {
  if (mutation.id.len == 0 || !impl_ || !impl_->apply) {
    return false;
  }
  auto* recv = new MutationReceiver(impl_->apply, mutation);
  return impl_->mgr.call_command(new base::SmtCommand(recv, true));
}

bool CommandEditSession::undo() {
  if (!impl_ || !impl_->mgr.can_undo()) {
    return false;
  }
  impl_->mgr.undo();
  return true;
}

bool CommandEditSession::redo() {
  if (!impl_ || !impl_->mgr.can_redo()) {
    return false;
  }
  impl_->mgr.redo();
  return true;
}

bool CommandEditSession::can_undo() const {
  return impl_ && impl_->mgr.can_undo();
}

bool CommandEditSession::can_redo() const {
  return impl_ && impl_->mgr.can_redo();
}

}  // namespace sdb
