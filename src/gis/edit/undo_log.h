// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_UNDO_LOG_H_
#define GIS_EDIT_UNDO_LOG_H_

#include <cstddef>
#include <vector>

#include "gis/edit/mutation.h"
#include "gis/gis_export.h"

namespace gis {

// Done / undone stacks for FeatureMutation. Callers apply the map (or store)
// change, then record or rewind through this log. Not a god-session.
class GIS_EXPORT UndoLog {
 public:
  void record(const FeatureMutation& mutation);
  bool undo_into(FeatureMutation* out);
  bool redo_into(FeatureMutation* out);

  bool can_undo() const;
  bool can_redo() const;

  size_t committed_count() const { return done_.size(); }
  bool committed_empty() const { return done_.empty(); }
  FeatureMutation committed_at(size_t index) const;

 private:
  std::vector<FeatureMutation> done_;
  std::vector<FeatureMutation> undone_;
};

}  // namespace gis

#endif  // GIS_EDIT_UNDO_LOG_H_
