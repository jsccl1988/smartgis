// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/memory_session.h"

namespace gis {

MemoryEditSession::MemoryEditSession() = default;

MemoryEditSession::MemoryEditSession(
    std::shared_ptr<OptimisticLayerStore> store)
    : store_(std::move(store)) {}

MemoryEditSession::~MemoryEditSession() = default;

bool MemoryEditSession::commit(const FeatureMutation& mutation) {
  last_conflict_.reset();
  if (mutation.id.len == 0) {
    last_status_ = CommitStatus::kRejected;
    return false;
  }
  if (store_) {
    ConflictError conflict;
    const CommitStatus st =
        store_->try_commit(mutation, mutation.base_version, &conflict);
    last_status_ = st;
    if (st == CommitStatus::kConflict) {
      last_conflict_ = conflict;
      return false;
    }
    if (st != CommitStatus::kOk) {
      return false;
    }
  } else {
    last_status_ = CommitStatus::kOk;
  }
  log_.record(mutation);
  return true;
}

CommitStatus MemoryEditSession::commit_optimistic(
    const FeatureMutation& mutation, uint64_t base_version,
    ConflictError* conflict) {
  FeatureMutation copy = mutation;
  copy.base_version = base_version;
  if (!commit(copy)) {
    if (conflict && last_conflict_) {
      *conflict = *last_conflict_;
    }
    return last_status_;
  }
  if (conflict) {
    *conflict = ConflictError{};
  }
  return CommitStatus::kOk;
}

bool MemoryEditSession::undo() {
  FeatureMutation unused;
  return log_.undo_into(&unused);
}

bool MemoryEditSession::redo() {
  FeatureMutation unused;
  return log_.redo_into(&unused);
}

bool MemoryEditSession::can_undo() const { return log_.can_undo(); }

bool MemoryEditSession::can_redo() const { return log_.can_redo(); }

OptimisticLayerStore* MemoryEditSession::store() { return store_.get(); }

const OptimisticLayerStore* MemoryEditSession::store() const {
  return store_.get();
}

size_t MemoryEditSession::committed_count() const {
  return log_.committed_count();
}

bool MemoryEditSession::committed_empty() const {
  return log_.committed_empty();
}

FeatureMutation MemoryEditSession::committed_at(size_t index) const {
  return log_.committed_at(index);
}

CommitStatus MemoryEditSession::last_status() const { return last_status_; }

const ConflictError* MemoryEditSession::last_conflict() const {
  return last_conflict_ ? &*last_conflict_ : nullptr;
}

}  // namespace gis
