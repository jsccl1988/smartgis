// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MODEL_EDIT_SESSION_COMMAND_EDIT_SESSION_H_
#define GIS_MODEL_EDIT_SESSION_COMMAND_EDIT_SESSION_H_

#include <functional>
#include <memory>

#include "gis/gis_export.h"
#include "gis/model/edit/session/edit_session.h"

namespace gis {

// Wraps leftover SmtCommandManager. apply(mutation, undo) performs the map
// change.
class GIS_EXPORT CommandEditSession : public EditSession {
 public:
  using ApplyFn = std::function<bool(const FeatureMutation&, bool undo)>;

  explicit CommandEditSession(ApplyFn apply);
  ~CommandEditSession() override;

  bool commit(const FeatureMutation& mutation) override;
  bool undo() override;
  bool redo() override;
  bool can_undo() const override;
  bool can_redo() const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gis

#endif  // GIS_MODEL_EDIT_SESSION_COMMAND_EDIT_SESSION_H_
