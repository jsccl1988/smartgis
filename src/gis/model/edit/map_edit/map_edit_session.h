// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MODEL_EDIT_MAP_EDIT_MAP_EDIT_SESSION_H_
#define GIS_MODEL_EDIT_MAP_EDIT_MAP_EDIT_SESSION_H_

#include <memory>

#include "gis/gis_export.h"
#include "gis/model/edit/session/command_edit_session.h"

class OGRFeature;

namespace gis {
class SmtMap;
}

namespace gis {

// Applies feature append, delete, and modify on SmtMap through the command log.
class GIS_EXPORT MapEditSession : public CommandEditSession {
 public:
  explicit MapEditSession(gis::SmtMap* map);
  ~MapEditSession() override;

  void bind_map(gis::SmtMap* map);

  bool commit_feature(EditOp op, OGRFeature* feature);

 private:
  bool apply_map(const FeatureMutation& mutation, bool undo);

  struct Impl;
  std::unique_ptr<Impl> impl_;
  gis::SmtMap* map_ = nullptr;
};

}  // namespace gis

#endif  // GIS_MODEL_EDIT_MAP_EDIT_MAP_EDIT_SESSION_H_
