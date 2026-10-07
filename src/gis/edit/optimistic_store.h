// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_OPTIMISTIC_STORE_H_
#define GIS_EDIT_OPTIMISTIC_STORE_H_

#include <cstdint>
#include <map>

#include "gis/edit/mutation.h"
#include "gis/feature/attrs.h"
#include "gis/gis_export.h"

namespace gis {

// Shared in-memory feature/layer version table for dual logical clients (M4).
// No PostGIS required — simulates optimistic concurrency on one layer.
class GIS_EXPORT OptimisticLayerStore {
 public:
  OptimisticLayerStore();
  ~OptimisticLayerStore();

  OptimisticLayerStore(const OptimisticLayerStore&) = delete;
  OptimisticLayerStore& operator=(const OptimisticLayerStore&) = delete;

  // Ensure |id| exists at |version| (default 1). Overwrites any prior token.
  void seed_feature(const FeatureId& id, uint64_t version = 1);

  // 0 when the feature has never been seeded or committed.
  uint64_t feature_version(const FeatureId& id) const;
  uint64_t layer_version() const { return layer_version_; }

  // Succeeds only when base_version == current feature version, then bumps
  // the feature token by 1 and increments layer_version_.
  CommitStatus try_commit(const FeatureMutation& mutation,
                          uint64_t base_version, ConflictError* conflict);

 private:
  struct FeatureIdLess {
    bool operator()(const FeatureId& a, const FeatureId& b) const;
  };

  std::map<FeatureId, uint64_t, FeatureIdLess> versions_;
  uint64_t layer_version_ = 0;
};

}  // namespace gis

#endif  // GIS_EDIT_OPTIMISTIC_STORE_H_
