// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_WORKSPACE_H_
#define TOOL_WORKSPACE_H_

#include <string_view>

#include "gis/edit/edit_session.h"
#include "tool/command.h"
#include "tool/gestures.h"
#include "tool/interaction.h"

namespace content {
class EventBus;
}

// Per-view composition root: commands, exclusive tool, input, optional edits.
namespace tool {

class Workspace {
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

  // Leftover chrome applies camera / select / digitize from the same draft.
  void set_draft_observer(DraftCallback observer);

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
  uint32_t pending_draft_flags_ = 0;
  bool flashing_ = false;
};

}  // namespace tool

#endif  // TOOL_WORKSPACE_H_
