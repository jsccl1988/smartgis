// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/optimistic_store.h"

#include <cstring>

namespace gis {

bool OptimisticLayerStore::FeatureIdLess::operator()(const FeatureId& a,
                                                     const FeatureId& b) const {
  if (a.len != b.len) {
    return a.len < b.len;
  }
  return std::memcmp(a.bytes, b.bytes, a.len) < 0;
}

OptimisticLayerStore::OptimisticLayerStore() = default;
OptimisticLayerStore::~OptimisticLayerStore() = default;

void OptimisticLayerStore::seed_feature(const FeatureId& id, uint64_t version) {
  if (id.len == 0) {
    return;
  }
  versions_[id] = version;
}

uint64_t OptimisticLayerStore::feature_version(const FeatureId& id) const {
  const auto it = versions_.find(id);
  if (it == versions_.end()) {
    return 0;
  }
  return it->second;
}

CommitStatus OptimisticLayerStore::try_commit(const FeatureMutation& mutation,
                                              uint64_t base_version,
                                              ConflictError* conflict) {
  if (mutation.id.len == 0) {
    return CommitStatus::kRejected;
  }
  const uint64_t current = feature_version(mutation.id);
  if (base_version != current) {
    if (conflict) {
      conflict->client_version = base_version;
      conflict->store_version = current;
      conflict->id = mutation.id;
    }
    return CommitStatus::kConflict;
  }
  versions_[mutation.id] = current + 1;
  ++layer_version_;
  return CommitStatus::kOk;
}

}  // namespace gis
