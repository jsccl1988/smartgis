// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/edit/edit_session.h"

namespace sdb {

bool MemoryEditSession::commit(const FeatureMutation& mutation) {
  if (mutation.id.len == 0) {
    return false;
  }
  done_.push_back(mutation);
  redo_.clear();
  return true;
}

bool MemoryEditSession::undo() {
  if (done_.empty()) {
    return false;
  }
  redo_.push_back(done_.back());
  done_.pop_back();
  return true;
}

bool MemoryEditSession::redo() {
  if (redo_.empty()) {
    return false;
  }
  done_.push_back(redo_.back());
  redo_.pop_back();
  return true;
}

bool MemoryEditSession::can_undo() const {
  return !done_.empty();
}

bool MemoryEditSession::can_redo() const {
  return !redo_.empty();
}

}  // namespace sdb
