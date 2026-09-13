// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/edit/edit_session.h"

#include "command.h"

namespace sdb {
namespace {

class MutationReceiver final : public Smt_Core::SmtCommandReceiver {
 public:
  MutationReceiver(CommandEditSession::ApplyFn apply, FeatureMutation mutation)
      : apply_(std::move(apply)), mutation_(mutation) {}

  bool Action(bool bUndo) override {
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
  Smt_Core::SmtCommandManager mgr;
};

CommandEditSession::CommandEditSession(ApplyFn apply)
    : impl_(std::make_unique<Impl>()) {
  impl_->apply = std::move(apply);
}

CommandEditSession::~CommandEditSession() {
  if (impl_) {
    impl_->mgr.ClearAllCommands();
  }
}

bool CommandEditSession::commit(const FeatureMutation& mutation) {
  if (mutation.id.len == 0 || !impl_ || !impl_->apply) {
    return false;
  }
  auto* recv = new MutationReceiver(impl_->apply, mutation);
  return impl_->mgr.CallCommand(new Smt_Core::SmtCommand(recv, true));
}

bool CommandEditSession::undo() {
  if (!impl_ || !impl_->mgr.CanUndo()) {
    return false;
  }
  impl_->mgr.Undo();
  return true;
}

bool CommandEditSession::redo() {
  if (!impl_ || !impl_->mgr.CanRedo()) {
    return false;
  }
  impl_->mgr.Redo();
  return true;
}

bool CommandEditSession::can_undo() const {
  return impl_ && impl_->mgr.CanUndo();
}

bool CommandEditSession::can_redo() const {
  return impl_ && impl_->mgr.CanRedo();
}

}  // namespace sdb
