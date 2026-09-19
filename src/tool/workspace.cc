// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/workspace.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "content/public/event_bus.h"
#include "content/public/events.h"
#include "content/public/map_types.h"
#include "tool/camera_nav.h"
#include "tool/gestures.h"

namespace tool {
namespace {

class WheelZoomDraft final : public Interaction {
 public:
  explicit WheelZoomDraft(DraftCallback cb) : cb_(std::move(cb)) {}
  const char* id() const override { return "wheel.zoom"; }
  bool on_input(const content::InputEvent& e) override {
    if (e.kind != content::InputEvent::Kind::kWheel) {
      return false;
    }
    if (!cb_) {
      return true;
    }
    if (content::is_horizontal_wheel(e)) {
      // Trackpad / mouse horizontal wheel → touch-pan shaped draft.
      const int32_t dx = tool::hwheel_pan_dx(e.wheel);
      if (dx == 0) {
        return true;
      }
      Draft d;
      d.kind = DraftKind::kRect;
      d.flags = draft_flags::kTouchPan;
      d.points.push_back({e.x_px, e.y_px});
      d.points.push_back({e.x_px + dx, e.y_px});
      cb_(d);
      return true;
    }
    Draft d;
    d.kind = DraftKind::kWheel;
    d.wheel = e.wheel;
    d.flags = e.flags;
    d.points.push_back({e.x_px, e.y_px});
    cb_(d);
    return true;
  }

 private:
  DraftCallback cb_;
};

}  // namespace

Workspace::Workspace(content::EventBus* events, gis::EditSession* edits)
    : events_(events), edits_(edits), dispatcher_(&catalog_) {
  router_.set_stack(&stack_);
  router_.add_always_on(std::make_unique<WheelZoomDraft>(bind_draft()));
  router_.add_always_on(make_hover_cursor());
  register_builtins();
}

void Workspace::set_draft_observer(DraftCallback observer) {
  draft_observer_ = std::move(observer);
}

bool Workspace::execute(std::string_view command_id, const CommandArgs& args) {
  return dispatcher_.execute(command_id, args);
}

bool Workspace::activate(std::string_view interaction_id) {
  return stack_.activate(interaction_id, interactions_);
}

bool Workspace::dispatch_input(const content::InputEvent& e) {
  Interaction* cur = stack_.current();
  if (cur) {
    const char* id = cur->id();
    if (id && std::strncmp(id, "view3d.", 7) == 0) {
      return cur->on_input(e);
    }
    // Drawing / select keep exclusive capture; navigate gets cursor-zoom.
    if (e.kind == content::InputEvent::Kind::kWheel && !is_navigate_tool(id)) {
      return cur->on_input(e);
    }
  }
  return router_.dispatch(e);
}

void Workspace::aux_draw() {
  if (Interaction* cur = stack_.current()) {
    cur->aux_draw();
  }
}

const AuxOverlay* Workspace::live_preview() const {
  if (Interaction* cur = stack_.current()) {
    return cur->aux_overlay();
  }
  return nullptr;
}

void Workspace::bind_activate(const char* command_id,
                              const char* interaction_id) {
  catalog_.add(command_id, [this, interaction_id](const CommandArgs&) {
    return activate(interaction_id);
  });
}

DraftCallback Workspace::bind_draft() {
  return [this](const Draft& draft) {
    Draft stamped = draft;
    if (stamped.flags == 0 && pending_draft_flags_ != 0) {
      stamped.flags = pending_draft_flags_;
    }
    on_draft(stamped);
  };
}

content::FeatureId Workspace::id_from_draft(const Draft& draft) {
  content::FeatureId id{};
  id.len = 1;
  id.bytes[0] = static_cast<uint8_t>(draft.kind);
  return id;
}

void Workspace::on_draft(const Draft& draft) {
  last_draft_ = draft;
  if (draft_observer_) {
    draft_observer_(draft);
  }
  if (draft.kind == DraftKind::kWheel) {
    if (events_) {
      events_->publish(content::ExtentChanged{});
    }
    return;
  }
  Interaction* cur = stack_.current();
  if (!cur) {
    return;
  }
  const char* id = cur->id();
  if (std::strncmp(id, "select.", 7) == 0) {
    if (events_) {
      content::SelectionChanged ev;
      ev.ids.push_back(id_from_draft(draft));
      events_->publish(ev);
    }
    return;
  }
  if (std::strncmp(id, "view3d.", 7) == 0) {
    if (draft.kind == DraftKind::kPick) {
      if (events_) {
        content::SelectionChanged ev;
        ev.ids.push_back(id_from_draft(draft));
        events_->publish(ev);
      }
      return;
    }
    if (events_) {
      events_->publish(content::ExtentChanged{});
    }
    return;
  }
  if (std::strncmp(id, "view.", 5) == 0) {
    if (events_) {
      events_->publish(content::ExtentChanged{});
    }
    return;
  }
  if (std::strncmp(id, "draw.", 5) == 0 && edits_) {
    gis::FeatureMutation mutation;
    mutation.op = gis::EditOp::kAppend;
    mutation.id = id_from_draft(draft);
    if (edits_->commit(mutation) && events_) {
      content::EditCommitted ev;
      ev.id = mutation.id;
      ev.op = content::EditCommitted::Op::kAppend;
      events_->publish(ev);
    }
  }
}

void Workspace::register_builtins() {
  DraftCallback cb = bind_draft();
  interactions_.add("view3d.trackball",
                    [cb]() { return make_view3d_trackball(cb); });
  interactions_.add("view3d.sphere", [cb]() { return make_view3d_sphere(cb); });
  interactions_.add("view3d.fps", [cb]() { return make_view3d_fps(cb); });
  interactions_.add("select.point", [cb]() { return make_select_point(cb); });
  interactions_.add("select.rect", [cb]() { return make_select_rect(cb); });
  interactions_.add("select.circle",
                    [cb]() { return make_select_circle(cb); });
  interactions_.add("select.polygon",
                    [cb]() { return make_select_polygon(cb); });
  interactions_.add("draw.point", [cb]() { return make_draw_point(cb); });
  interactions_.add("draw.linestring",
                    [cb]() { return make_draw_linestring(cb); });
  interactions_.add("draw.polygon", [cb]() { return make_draw_polygon(cb); });
  interactions_.add("draw.rect", [cb]() { return make_draw_rect(cb); });
  interactions_.add("view.zoom_in", [cb]() { return make_view_zoom_in(cb); });
  interactions_.add("view.zoom_out", [cb]() { return make_view_zoom_out(cb); });
  interactions_.add("view.pan", [cb]() { return make_view_pan(cb); });

  bind_activate("view.zoom_in", "view.zoom_in");
  bind_activate("view.zoom_out", "view.zoom_out");
  bind_activate("view.pan", "view.pan");
  bind_activate("view3d.trackball", "view3d.trackball");
  bind_activate("view3d.sphere", "view3d.sphere");
  bind_activate("view3d.fps", "view3d.fps");
  bind_activate("selection.point", "select.point");
  bind_activate("selection.rect", "select.rect");
  bind_activate("selection.polygon", "select.polygon");
  bind_activate("edit.append.point", "draw.point");
  bind_activate("edit.append.linestring", "draw.linestring");
  bind_activate("edit.append.polygon", "draw.polygon");

  catalog_.add("view.full", [this](const CommandArgs& args) {
    if (events_) {
      content::ExtentChanged ev;
      ev.view_id = args.view_id;
      events_->publish(ev);
    }
    return true;
  });
  catalog_.add("view.refresh", [this](const CommandArgs& args) {
    if (events_) {
      content::ExtentChanged ev;
      ev.view_id = args.view_id;
      events_->publish(ev);
    }
    return true;
  });
  catalog_.add("view.backend.rhi", [this](const CommandArgs& args) {
    if (events_) {
      content::RenderBackendChanged ev;
      ev.view_id = args.view_id;
      ev.kind = 0;
      events_->publish(ev);
    }
    return true;
  });
  catalog_.add("view.backend.maplibre", [this](const CommandArgs& args) {
    if (events_) {
      content::RenderBackendChanged ev;
      ev.view_id = args.view_id;
      ev.kind = 1;
      events_->publish(ev);
    }
    return true;
  });
  catalog_.add("view3d.full", [this](const CommandArgs& args) {
    if (events_) {
      content::ExtentChanged ev;
      ev.view_id = args.view_id;
      events_->publish(ev);
    }
    return true;
  });
  catalog_.add("flash.start", [this](const CommandArgs&) {
    flashing_ = true;
    return true;
  });
  catalog_.add("flash.stop", [this](const CommandArgs&) {
    flashing_ = false;
    return true;
  });

  catalog_.add("selection.clear", [this](const CommandArgs& args) {
    last_draft_ = Draft{};
    pending_draft_flags_ = 0;
    if (events_) {
      content::SelectionChanged ev;
      ev.view_id = args.view_id;
      events_->publish(ev);
    }
    return true;
  });

  catalog_.add("edit.undo", [this](const CommandArgs&) {
    return edits_ && edits_->can_undo() && edits_->undo();
  });
  catalog_.add("edit.redo", [this](const CommandArgs&) {
    return edits_ && edits_->can_redo() && edits_->redo();
  });
  catalog_.add("edit.cancel", [this](const CommandArgs&) {
    last_draft_ = Draft{};
    pending_draft_flags_ = 0;
    while (stack_.pop()) {
    }
    return true;
  });
}

}  // namespace tool
