// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_EDIT_EDIT_SESSION_H_
#define SDB_EDIT_EDIT_SESSION_H_

#include <functional>
#include <memory>
#include <vector>

#include "content/public/map_types.h"
#include "gis/gis_export.h"

// Undoable document mutations. Interactions never call this internally.
namespace gis {

enum class EditOp { kAppend, kDelete, kModify };

struct FeatureMutation {
  EditOp op = EditOp::kAppend;
  content::FeatureId id{};
  void* leftover = nullptr;
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

// In-memory log for tests and hosts that do not yet wrap SmtMap.
class GIS_EXPORT MemoryEditSession : public EditSession {
 public:
  MemoryEditSession();
  ~MemoryEditSession() override;

  bool commit(const FeatureMutation& mutation) override;
  bool undo() override;
  bool redo() override;
  bool can_undo() const override;
  bool can_redo() const override;

  const std::vector<FeatureMutation>& committed() const { return done_; }

 private:
  std::vector<FeatureMutation> done_;
  std::vector<FeatureMutation> redo_;
};

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

#endif  // SDB_EDIT_EDIT_SESSION_H_
