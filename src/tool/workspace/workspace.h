// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_WORKSPACE_H_
#define TOOL_WORKSPACE_H_

#include <functional>
#include <string_view>

#include "gis/model/edit/session/edit_session.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/tool_export.h"

namespace content {
class EventBus;
}

// Per-view composition root: commands, exclusive tool, input, optional edits.
namespace tool {

class SMT_TOOL_EXPORT Workspace {
 public:
  Workspace(content::EventBus* events, gis::EditSession* edits);

  CommandCatalog& catalog() { return catalog_; }
  CommandDispatcher& dispatcher() { return dispatcher_; }
  InteractionRegistry& interactions() { return interactions_; }
  InteractionStack& stack() { return stack_; }
  InputRouter& router() { return router_; }

  const Draft& last_draft() const { return last_draft_; }
  bool flashing() const { return flashing_; }

  // Pending fine subtype stamped onto drafts when Interaction leaves flags=0.
  void set_draft_flags(uint32_t flags) { pending_draft_flags_ = flags; }
  uint32_t draft_flags() const { return pending_draft_flags_; }

  // Leftover shell applies camera / select / digitize from the same draft.
  void set_draft_observer(DraftCallback observer);

  // Views hit-test authority. select.* and edit.vertex use this FeatureId.
  // Empty len is a miss. Never synthesize an id from DraftKind.
  using FeatureHit = std::function<content::FeatureId(const Draft&)>;
  void set_feature_hit(FeatureHit fn);

  // view.full / view.refresh move the camera, then publish the new extent.
  using NavCommand = std::function<content::Extent2(std::string_view command_id)>;
  void set_nav_command(NavCommand fn);

  // When set, draw.* geometry is written only by the shell observer
  // (MapScene::append_from_draft). EditSession is not a second writer.
  void set_shell_owns_append(bool on);

  // Rubber-band geometry for leftover paint. No HWND on this header.
  void aux_draw();
  const AuxOverlay* live_preview() const;

  bool execute(std::string_view command_id, const CommandArgs& args);
  bool activate(std::string_view interaction_id);
  // 3D exclusive wheel is view3d.*, not always-on wheel.zoom.
  bool dispatch_input(const content::InputEvent& e);

 private:
  void register_builtins();
  void bind_activate(const char* command_id, const char* interaction_id);
  DraftCallback bind_draft();
  void on_draft(const Draft& draft);
  static content::FeatureId id_from_draft(const Draft& draft);

  content::EventBus* events_ = nullptr;
  gis::EditSession* edits_ = nullptr;
  CommandCatalog catalog_;
  CommandDispatcher dispatcher_;
  InteractionRegistry interactions_;
  InteractionStack stack_;
  InputRouter router_;
  Draft last_draft_{};
  DraftCallback draft_observer_;
  FeatureHit feature_hit_;
  NavCommand nav_command_;
  uint32_t pending_draft_flags_ = 0;
  bool flashing_ = false;
  bool shell_owns_append_ = false;
};

}  // namespace tool

#endif  // TOOL_WORKSPACE_H_
