// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/undo_log.h"

namespace gis {

void UndoLog::record(const FeatureMutation& mutation) {
  done_.push_back(mutation);
  undone_.clear();
}

bool UndoLog::undo_into(FeatureMutation* out) {
  if (done_.empty() || !out) {
    return false;
  }
  *out = done_.back();
  done_.pop_back();
  undone_.push_back(*out);
  return true;
}

bool UndoLog::redo_into(FeatureMutation* out) {
  if (undone_.empty() || !out) {
    return false;
  }
  *out = undone_.back();
  undone_.pop_back();
  done_.push_back(*out);
  return true;
}

bool UndoLog::can_undo() const { return !done_.empty(); }

bool UndoLog::can_redo() const { return !undone_.empty(); }

FeatureMutation UndoLog::committed_at(size_t index) const {
  if (index >= done_.size()) {
    return FeatureMutation{};
  }
  return done_[index];
}

}  // namespace gis
