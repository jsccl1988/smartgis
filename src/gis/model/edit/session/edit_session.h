// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MODEL_EDIT_SESSION_EDIT_SESSION_H_
#define GIS_MODEL_EDIT_SESSION_EDIT_SESSION_H_

#include <cstdint>

#include "content/public/map_types.h"
#include "gis/gis_export.h"

// Undoable document mutations. Interactions never call this internally.
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
};

class GIS_EXPORT EditSession {
 public:
  virtual ~EditSession() = default;
  virtual bool commit(const FeatureMutation& mutation) = 0;
  virtual bool undo() = 0;
  virtual bool redo() = 0;
  virtual bool can_undo() const = 0;
  virtual bool can_redo() const = 0;
};

}  // namespace gis

#endif  // GIS_MODEL_EDIT_SESSION_EDIT_SESSION_H_
