// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_EDIT_MAP_EDIT_SESSION_H_
#define SDB_EDIT_MAP_EDIT_SESSION_H_

#include "gis/edit/edit_session.h"

class OGRFeature;

namespace gis {
class SmtMap;
}

namespace gis {

class GIS_EXPORT MapEditSession : public CommandEditSession {
 public:
  explicit MapEditSession(gis::SmtMap* map);
  void bind_map(gis::SmtMap* map);

  bool commit_feature(EditOp op, OGRFeature* feature);

 private:
  bool apply_map(const FeatureMutation& mutation, bool undo);

  gis::SmtMap* map_ = nullptr;
};

}  // namespace gis

#endif  // SDB_EDIT_MAP_EDIT_SESSION_H_
