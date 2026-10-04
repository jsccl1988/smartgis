// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_MAP_SESSION_H_
#define GIS_EDIT_MAP_SESSION_H_

#include <memory>

#include "gis/edit/command_session.h"
#include "gis/gis_export.h"

class OGRFeature;

namespace gis {
class Map;

// Binds FeatureMutation apply to gis::Map. Composes CommandEditSession for
// the undo log instead of inheriting it.
class GIS_EXPORT MapEditSession : public EditSession {
 public:
  explicit MapEditSession(Map* map);
  ~MapEditSession() override;

  void bind_map(Map* map);

  bool commit(const FeatureMutation& mutation) override;
  bool undo() override;
  bool redo() override;
  bool can_undo() const override;
  bool can_redo() const override;

  bool commit_feature(EditOp op, OGRFeature* feature);

 private:
  bool apply_map(const FeatureMutation& mutation, bool undo);
  OGRFeature* build_feature_from_geom(const FeatureGeom& geom);

  CommandEditSession commands_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  Map* map_ = nullptr;
};

}  // namespace gis

#endif  // GIS_EDIT_MAP_SESSION_H_
