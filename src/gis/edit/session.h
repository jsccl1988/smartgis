// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_SESSION_H_
#define GIS_EDIT_SESSION_H_

#include "gis/edit/mutation.h"
#include "gis/gis_export.h"

// Undoable document mutations. Interactions never call this internally.
namespace gis {

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

#endif  // GIS_EDIT_SESSION_H_
