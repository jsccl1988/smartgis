// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/workspace/draft_pipeline.h"

#include <cstring>

#include "content/public/event_bus.h"
#include "gis/model/edit/session/edit_session.h"
#include "tool/interaction/interaction.h"

namespace tool {
namespace detail {

void DraftPipeline::set_observer(DraftCallback observer) {
  observer_ = std::move(observer);
}

void DraftPipeline::set_feature_hit(FeatureHit fn) {
  feature_hit_ = std::move(fn);
}

void DraftPipeline::set_shell_owns_append(bool on) {
  shell_owns_append_ = on;
}

void DraftPipeline::set_pending_flags(uint32_t flags) {
  pending_flags_ = flags;
}

void DraftPipeline::clear_last_draft() {
  last_draft_ = Draft{};
  pending_flags_ = 0;
}

content::FeatureId DraftPipeline::id_from_draft(const Draft& draft) {
  content::FeatureId id{};
  id.len = 1;
  id.bytes[0] = static_cast<uint8_t>(draft.kind);
  return id;
}

void DraftPipeline::on_draft(const Draft& draft, Interaction* current,
                             content::EventBus* events,
                             gis::EditSession* edits) {
  Draft stamped = draft;
  if (stamped.flags == 0 && pending_flags_ != 0) {
    stamped.flags = pending_flags_;
  }
  last_draft_ = stamped;

  const char* id = current ? current->id() : "";
  content::FeatureId resolved{};
  const bool select_tool = id && std::strncmp(id, "select.", 7) == 0;
  const bool vertex_tool = id && std::strcmp(id, "edit.vertex") == 0;
  if (feature_hit_ && (select_tool || vertex_tool)) {
    resolved = feature_hit_(stamped);
  }
  if (observer_) {
    observer_(stamped);
  }
  if (stamped.kind == DraftKind::kWheel) {
    if (events) {
      events->publish(content::ExtentChanged{});
    }
    return;
  }
  if (!current) {
    return;
  }
  if (select_tool) {
    if (events && feature_hit_) {
      content::SelectionChanged ev;
      if (resolved.len > 0) {
        ev.ids.push_back(resolved);
      }
      events->publish(ev);
    }
    return;
  }
  if (vertex_tool) {
    if (resolved.len == 0 || !edits) {
      return;
    }
    gis::FeatureMutation mutation;
    mutation.op = gis::EditOp::kModify;
    mutation.id = resolved;
    if (edits->commit(mutation) && events) {
      content::EditCommitted ev;
      ev.id = mutation.id;
      ev.op = content::EditCommitted::Op::kModify;
      events->publish(ev);
    }
    return;
  }
  if (std::strncmp(id, "view3d.", 7) == 0) {
    if (stamped.kind == DraftKind::kPick) {
      if (events) {
        content::SelectionChanged ev;
        ev.ids.push_back(id_from_draft(stamped));
        events->publish(ev);
      }
      return;
    }
    if (events) {
      events->publish(content::ExtentChanged{});
    }
    return;
  }
  if (std::strncmp(id, "view.", 5) == 0) {
    if (events) {
      events->publish(content::ExtentChanged{});
    }
    return;
  }
  if (std::strncmp(id, "draw.", 5) == 0) {
    if (shell_owns_append_) {
      return;
    }
    if (!edits) {
      return;
    }
    gis::FeatureMutation mutation;
    mutation.op = gis::EditOp::kAppend;
    mutation.id = id_from_draft(stamped);
    if (edits->commit(mutation) && events) {
      content::EditCommitted ev;
      ev.id = mutation.id;
      ev.op = content::EditCommitted::Op::kAppend;
      events->publish(ev);
    }
  }
}

}  // namespace detail
}  // namespace tool
