// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_MUTATION_H_
#define GIS_EDIT_MUTATION_H_

#include <cstdint>
#include <vector>

#include "content/public/map_layer_types.h"

namespace gis {

enum class EditOp { kAppend, kDelete, kModify };

// Result of an optimistic (version-token) commit against a shared store.
enum class CommitStatus {
  kOk = 0,
  kRejected = 1,
  kConflict = 2,
};

// Filled when CommitStatus::kConflict — client held a stale version token.
struct ConflictError {
  uint64_t client_version = 0;
  uint64_t store_version = 0;
  content::FeatureId id{};
};

// Map-CRS vertex for FeatureGeom (no OGR on this header).
struct MapVertex {
  double x = 0;
  double y = 0;
};

// Optional geometry payload for append/modify. Empty kind = id-only stub
// (tests / hosts that resolve geometry elsewhere).
struct FeatureGeom {
  enum class Kind { kNone = 0, kPoint, kLineString, kPolygon };

  Kind kind = Kind::kNone;
  // Fine digitize subtype from tool::draft_flags (family<<16)|code.
  uint32_t flags = 0;
  std::vector<MapVertex> points;

  bool empty() const { return kind == Kind::kNone || points.empty(); }
};

// One logged feature change. host_token is copied through undo/redo so a host
// can recover private state; 0 means the host stored nothing.
struct FeatureMutation {
  EditOp op = EditOp::kAppend;
  content::FeatureId id{};
  // Optimistic base version. When the session is bound to an
  // OptimisticLayerStore, must equal the store version for this feature
  // (seeded features typically start at 1). Ignored when no store is bound.
  uint64_t base_version = 0;
  uint64_t host_token = 0;
  FeatureGeom geom{};
};

}  // namespace gis

#endif  // GIS_EDIT_MUTATION_H_
