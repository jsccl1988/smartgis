// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_WORKSPACE_DRAFT_PIPELINE_H_
#define TOOL_WORKSPACE_DRAFT_PIPELINE_H_

#include <cstdint>
#include <functional>

#include "content/public/map_types.h"
#include "tool/draft/draft.h"

namespace content {
class EventBus;
}

namespace gis {
class EditSession;
}

namespace tool {

class Interaction;

namespace detail {

// Owns draft stamping, shell observer, feature hit, and document side-effects.
// Workspace wires this; Interactions only emit Draft via DraftCallback.
class DraftPipeline {
 public:
  using FeatureHit = std::function<content::FeatureId(const Draft&)>;

  void set_observer(DraftCallback observer);
  void set_feature_hit(FeatureHit fn);
  void set_shell_owns_append(bool on);

  void set_pending_flags(uint32_t flags);
  uint32_t pending_flags() const { return pending_flags_; }

  const Draft& last_draft() const { return last_draft_; }
  void clear_last_draft();

  // Stamp pending flags when Interaction left flags=0, then apply.
  void on_draft(const Draft& draft, Interaction* current,
                content::EventBus* events, gis::EditSession* edits);

 private:
  static content::FeatureId id_from_draft(const Draft& draft);

  Draft last_draft_{};
  DraftCallback observer_;
  FeatureHit feature_hit_;
  uint32_t pending_flags_ = 0;
  bool shell_owns_append_ = false;
};

}  // namespace detail
}  // namespace tool

#endif  // TOOL_WORKSPACE_DRAFT_PIPELINE_H_
