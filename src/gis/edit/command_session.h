// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_COMMAND_SESSION_H_
#define GIS_EDIT_COMMAND_SESSION_H_

#include <functional>
#include <memory>

#include "gis/edit/session.h"
#include "gis/gis_export.h"

namespace gis {

// Undo/redo log plus an apply callback. apply(mutation, undo) performs the
// document change; this type only owns the command stacks.
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

#endif  // GIS_EDIT_COMMAND_SESSION_H_
