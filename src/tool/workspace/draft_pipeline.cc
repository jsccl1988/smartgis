// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/workspace/draft_pipeline.h"

#include <cstring>

#include "content/public/event_bus.h"
#include "gis/edit/session.h"
#include "tool/interaction/interaction.h"

namespace tool {
namespace detail {
namespace {

gis::FeatureGeom::Kind kind_from_draft(DraftKind kind) {
  switch (kind) {
    case DraftKind::kLineString:
      return gis::FeatureGeom::Kind::kLineString;
    case DraftKind::kPolygon:
    case DraftKind::kRect:
      return gis::FeatureGeom::Kind::kPolygon;
    case DraftKind::kPoint:
    default:
      return gis::FeatureGeom::Kind::kPoint;
  }
}

}  // namespace

void DraftPipeline::set_observer(DraftCallback observer) {
  observer_ = std::move(observer);
}

void DraftPipeline::set_feature_hit(FeatureHit fn) {
  feature_hit_ = std::move(fn);
}

void DraftPipeline::set_map_project(MapProject fn) {
  map_project_ = std::move(fn);
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

gis::FeatureGeom DraftPipeline::geom_from_draft(const Draft& draft,
                                                const MapProject& project) {
  gis::FeatureGeom geom;
  if (draft.points.empty()) {
    return geom;
  }
  geom.kind = kind_from_draft(draft.kind);
  geom.flags = draft.flags;

  auto push_px = [&](int32_t x_px, int32_t y_px) {
    double mx = static_cast<double>(x_px);
    double my = static_cast<double>(y_px);
    if (project) {
      project(x_px, y_px, &mx, &my);
    }
    geom.points.push_back({mx, my});
  };

  if (draft.kind == DraftKind::kRect && draft.points.size() >= 2) {
    const int32_t x0 = draft.points[0].x_px;
    const int32_t y0 = draft.points[0].y_px;
    const int32_t x1 = draft.points[1].x_px;
    const int32_t y1 = draft.points[1].y_px;
    push_px(x0, y0);
    push_px(x1, y0);
    push_px(x1, y1);
    push_px(x0, y1);
    push_px(x0, y0);
    return geom;
  }

  for (const DraftPoint& p : draft.points) {
    push_px(p.x_px, p.y_px);
  }
  return geom;
}

void DraftPipeline::on_draft(const Draft& draft, Interaction* current,
                             content::EventBus* events,
                             gis::EditSession* edits) {
  Draft stamped = draft;
  // StrokeInteraction always ORs kGestureEnd on pointer-up, so flags != 0 even
  // when family/code are unset. Stamp pending when the packed family is empty.
  if (draft_flags::family_of(stamped.flags) == draft_flags::kFamilyNone &&
      pending_flags_ != 0) {
    stamped.flags |= pending_flags_;
  }
  last_draft_ = stamped;

  const char* id = current ? current->id() : "";
  content::FeatureId resolved{};
  const bool select_tool = id && std::strncmp(id, "select.", 7) == 0;
  const bool vertex_tool = id && std::strcmp(id, "edit.vertex") == 0;
  const bool draw_tool = id && std::strncmp(id, "draw.", 5) == 0;

  // draw.*: commit FeatureGeom first so leftover/shell observers see a logged
  // mutation and do not double-write geometry.
  if (draw_tool && !shell_owns_append_ && edits && !stamped.points.empty()) {
    gis::FeatureMutation mutation;
    mutation.op = gis::EditOp::kAppend;
    mutation.id = id_from_draft(stamped);
    mutation.geom = geom_from_draft(stamped, map_project_);
    if (!mutation.geom.empty() && edits->commit(mutation) && events) {
      content::EditCommitted ev;
      ev.id = mutation.id;
      ev.op = content::EditCommitted::Op::kAppend;
      events->publish(ev);
    }
  }

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
  // draw.* already committed above when !shell_owns_append_.
}

}  // namespace detail
}  // namespace tool
