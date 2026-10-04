// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/command_session.h"

#include <utility>

#include "gis/edit/undo_log.h"

namespace gis {

struct CommandEditSession::Impl {
  ApplyFn apply;
  UndoLog log;
};

CommandEditSession::CommandEditSession(ApplyFn apply)
    : impl_(std::make_unique<Impl>()) {
  impl_->apply = std::move(apply);
}

CommandEditSession::~CommandEditSession() = default;

bool CommandEditSession::commit(const FeatureMutation& mutation) {
  if (mutation.id.len == 0 || !impl_ || !impl_->apply) {
    return false;
  }
  if (!impl_->apply(mutation, false)) {
    return false;
  }
  impl_->log.record(mutation);
  return true;
}

bool CommandEditSession::undo() {
  if (!impl_ || !impl_->apply || !impl_->log.can_undo()) {
    return false;
  }
  FeatureMutation mutation;
  if (!impl_->log.undo_into(&mutation)) {
    return false;
  }
  if (!impl_->apply(mutation, true)) {
    impl_->log.redo_into(&mutation);
    return false;
  }
  return true;
}

bool CommandEditSession::redo() {
  if (!impl_ || !impl_->apply || !impl_->log.can_redo()) {
    return false;
  }
  FeatureMutation mutation;
  if (!impl_->log.redo_into(&mutation)) {
    return false;
  }
  if (!impl_->apply(mutation, false)) {
    impl_->log.undo_into(&mutation);
    return false;
  }
  return true;
}

bool CommandEditSession::can_undo() const {
  return impl_ && impl_->log.can_undo();
}

bool CommandEditSession::can_redo() const {
  return impl_ && impl_->log.can_redo();
}

}  // namespace gis
