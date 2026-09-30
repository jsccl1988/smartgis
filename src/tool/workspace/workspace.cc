// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/workspace/workspace.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "content/public/event_bus.h"
#include "content/public/map_types.h"
#include "gis/model/edit/session/edit_session.h"
#include "tool/draft/draft.h"
#include "tool/nav/camera_nav.h"
#include "tool/workspace/draft_pipeline.h"
#include "tool/workspace/nav_bridge.h"

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

struct Workspace::Impl {
  content::EventBus* events = nullptr;
  gis::EditSession* edits = nullptr;
  CommandCatalog catalog;
  CommandDispatcher dispatcher{&catalog};
  InteractionRegistry interactions;
  InteractionStack stack;
  InputRouter router;
  detail::DraftPipeline drafts;
  detail::NavBridge nav;
  bool flashing = false;

  DraftCallback bind_draft() {
    return [this](const Draft& draft) {
      drafts.on_draft(draft, stack.current(), events, edits);
    };
  }

  void bind_activate(const char* command_id, const char* interaction_id) {
    catalog.add(command_id, [this, interaction_id](const CommandArgs&) {
      return stack.activate(interaction_id, interactions);
    });
  }

  void register_builtins() {
    DraftCallback cb = bind_draft();
    interactions.add("view3d.trackball",
                     [cb]() { return make_view3d_trackball(cb); });
    interactions.add("view3d.sphere", [cb]() { return make_view3d_sphere(cb); });
    interactions.add("view3d.fps", [cb]() { return make_view3d_fps(cb); });
    interactions.add("select.point", [cb]() { return make_select_point(cb); });
    interactions.add("select.rect", [cb]() { return make_select_rect(cb); });
    interactions.add("select.circle",
                     [cb]() { return make_select_circle(cb); });
    interactions.add("select.polygon",
                     [cb]() { return make_select_polygon(cb); });
    interactions.add("draw.point", [cb]() { return make_draw_point(cb); });
    interactions.add("draw.linestring",
                     [cb]() { return make_draw_linestring(cb); });
    interactions.add("draw.polygon", [cb]() { return make_draw_polygon(cb); });
    interactions.add("draw.rect", [cb]() { return make_draw_rect(cb); });
    interactions.add("view.zoom_in", [cb]() { return make_view_zoom_in(cb); });
    interactions.add("view.zoom_out",
                     [cb]() { return make_view_zoom_out(cb); });
    interactions.add("view.pan", [cb]() { return make_view_pan(cb); });

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
    interactions.add("edit.vertex", [cb]() { return make_edit_vertex(cb); });
    bind_activate("edit.vertex", "edit.vertex");

    catalog.add("view.full", [this](const CommandArgs& args) {
      return nav.publish_nav("view.full", args, events);
    });
    catalog.add("view.refresh", [this](const CommandArgs& args) {
      return nav.publish_nav("view.refresh", args, events);
    });
    catalog.add("view.backend.rhi", [this](const CommandArgs& args) {
      if (events) {
        content::RenderBackendChanged ev;
        ev.view_id = args.view_id;
        ev.kind = 0;
        events->publish(ev);
      }
      return true;
    });
    catalog.add("view.backend.maplibre", [this](const CommandArgs& args) {
      if (events) {
        content::RenderBackendChanged ev;
        ev.view_id = args.view_id;
        ev.kind = 1;
        events->publish(ev);
      }
      return true;
    });
    catalog.add("view3d.full", [this](const CommandArgs& args) {
      if (events) {
        content::ExtentChanged ev;
        ev.view_id = args.view_id;
        events->publish(ev);
      }
      return true;
    });
    catalog.add("flash.start", [this](const CommandArgs&) {
      flashing = true;
      return true;
    });
    catalog.add("flash.stop", [this](const CommandArgs&) {
      flashing = false;
      return true;
    });

    catalog.add("selection.clear", [this](const CommandArgs& args) {
      drafts.clear_last_draft();
      if (events) {
        content::SelectionChanged ev;
        ev.view_id = args.view_id;
        events->publish(ev);
      }
      return true;
    });

    catalog.add("edit.undo", [this](const CommandArgs&) {
      return edits && edits->can_undo() && edits->undo();
    });
    catalog.add("edit.redo", [this](const CommandArgs&) {
      return edits && edits->can_redo() && edits->redo();
    });
    catalog.add("edit.cancel", [this](const CommandArgs&) {
      drafts.clear_last_draft();
      while (stack.pop()) {
      }
      return true;
    });
  }
};

Workspace::Workspace(content::EventBus* events, gis::EditSession* edits)
    : impl_(std::make_unique<Impl>()) {
  impl_->events = events;
  impl_->edits = edits;
  impl_->router.set_stack(&impl_->stack);
  impl_->router.add_always_on(
      std::make_unique<WheelZoomDraft>(impl_->bind_draft()));
  impl_->router.add_always_on(make_hover_cursor());
  impl_->register_builtins();
}

Workspace::~Workspace() = default;

CommandCatalog& Workspace::catalog() {
  return impl_->catalog;
}

CommandDispatcher& Workspace::dispatcher() {
  return impl_->dispatcher;
}

InteractionRegistry& Workspace::interactions() {
  return impl_->interactions;
}

InteractionStack& Workspace::stack() {
  return impl_->stack;
}

InputRouter& Workspace::router() {
  return impl_->router;
}

const Draft& Workspace::last_draft() const {
  return impl_->drafts.last_draft();
}

bool Workspace::flashing() const {
  return impl_ && impl_->flashing;
}

void Workspace::set_draft_flags(uint32_t flags) {
  impl_->drafts.set_pending_flags(flags);
}

uint32_t Workspace::draft_flags() const {
  return impl_->drafts.pending_flags();
}

void Workspace::set_draft_observer(DraftCallback observer) {
  impl_->drafts.set_observer(std::move(observer));
}

void Workspace::set_feature_hit(FeatureHit fn) {
  impl_->drafts.set_feature_hit(std::move(fn));
}

void Workspace::set_nav_command(NavCommand fn) {
  impl_->nav.set_nav_command(std::move(fn));
}

void Workspace::set_map_project(MapProject fn) {
  impl_->drafts.set_map_project(std::move(fn));
}

void Workspace::set_shell_owns_append(bool on) {
  impl_->drafts.set_shell_owns_append(on);
}

bool Workspace::execute(std::string_view command_id, const CommandArgs& args) {
  return impl_->dispatcher.execute(command_id, args);
}

bool Workspace::activate(std::string_view interaction_id) {
  return impl_->stack.activate(interaction_id, impl_->interactions);
}

bool Workspace::dispatch_input(const content::InputEvent& e) {
  Interaction* cur = impl_->stack.current();
  if (cur) {
    const char* id = cur->id();
    if (id && std::strncmp(id, "view3d.", 7) == 0) {
      return cur->on_input(e);
    }
    if (e.kind == content::InputEvent::Kind::kWheel && !is_navigate_tool(id)) {
      return cur->on_input(e);
    }
  }
  return impl_->router.dispatch(e);
}

void Workspace::aux_draw() {
  if (Interaction* cur = impl_->stack.current()) {
    cur->aux_draw();
  }
}

const AuxOverlay* Workspace::live_preview() const {
  if (Interaction* cur = impl_->stack.current()) {
    return cur->aux_overlay();
  }
  return nullptr;
}

}  // namespace tool
