// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_EDIT_MAP_EDIT_SESSION_H_
#define SDB_EDIT_MAP_EDIT_SESSION_H_

#include "sdb/edit/edit_session.h"

class OGRFeature;

namespace sdb {
class SmtMap;
}

namespace sdb {

class GIS_EXPORT MapEditSession : public CommandEditSession {
 public:
  explicit MapEditSession(sdb::SmtMap* map);
  void bind_map(sdb::SmtMap* map);

  bool commit_feature(EditOp op, OGRFeature* feature);

 private:
  bool apply_map(const FeatureMutation& mutation, bool undo);

  sdb::SmtMap* map_ = nullptr;
};

}  // namespace sdb

#endif  // SDB_EDIT_MAP_EDIT_SESSION_H_
