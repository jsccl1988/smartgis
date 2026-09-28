// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MODEL_EDIT_SESSION_MEMORY_EDIT_SESSION_H_
#define GIS_MODEL_EDIT_SESSION_MEMORY_EDIT_SESSION_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "gis/gis_export.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/model/edit/session/optimistic_layer_store.h"

namespace gis {

// In-memory log for tests and hosts that do not yet wrap SmtMap.
// Optionally shares an OptimisticLayerStore so two sessions act as dual writers.
class GIS_EXPORT MemoryEditSession : public EditSession {
 public:
  MemoryEditSession();
  explicit MemoryEditSession(std::shared_ptr<OptimisticLayerStore> store);
  ~MemoryEditSession() override;

  bool commit(const FeatureMutation& mutation) override;
  bool undo() override;
  bool redo() override;
  bool can_undo() const override;
  bool can_redo() const override;

  // Explicit optimistic path; fills |conflict| on kConflict.
  CommitStatus commit_optimistic(const FeatureMutation& mutation,
                                 uint64_t base_version,
                                 ConflictError* conflict = nullptr);

  // Non-inline: STL members must be touched inside the gis DLL (MSVC C4251 /
  // LNK2005 when GIS_EXPORT inlines leak into every including TU). Do not return
  // STL container references across the DLL boundary — debug CRT aborts / AVs.
  OptimisticLayerStore* store();
  const OptimisticLayerStore* store() const;
  size_t committed_count() const;
  bool committed_empty() const;
  // By-value copy; empty id when |index| is out of range.
  FeatureMutation committed_at(size_t index) const;
  CommitStatus last_status() const;
  const ConflictError* last_conflict() const;

 private:
  std::shared_ptr<OptimisticLayerStore> store_;
  std::vector<FeatureMutation> done_;
  std::vector<FeatureMutation> redo_;
  CommitStatus last_status_ = CommitStatus::kOk;
  std::optional<ConflictError> last_conflict_;
};

}  // namespace gis

#endif  // GIS_MODEL_EDIT_SESSION_MEMORY_EDIT_SESSION_H_
