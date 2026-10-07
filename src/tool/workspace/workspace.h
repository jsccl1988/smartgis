// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_WORKSPACE_H_
#define TOOL_WORKSPACE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "content/public/types.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/tool_export.h"

namespace content {
class EventBus;
}

namespace gis {
class EditSession;
}

// Per-view composition root: commands, exclusive tool, input, optional edits.
// Methods exported; class not — avoids C4251 on pimpl.
namespace tool {

class Workspace {
 public:
  TOOL_EXPORT Workspace(content::EventBus* events, gis::EditSession* edits);
  TOOL_EXPORT ~Workspace();

  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;

  TOOL_EXPORT CommandCatalog& catalog();
  TOOL_EXPORT CommandDispatcher& dispatcher();
  TOOL_EXPORT InteractionRegistry& interactions();
  TOOL_EXPORT InteractionStack& stack();
  TOOL_EXPORT InputRouter& router();

  TOOL_EXPORT const Draft& last_draft() const;
  TOOL_EXPORT bool flashing() const;

  // Pending fine subtype stamped onto drafts when Interaction leaves flags=0.
  TOOL_EXPORT void set_draft_flags(uint32_t flags);
  TOOL_EXPORT uint32_t draft_flags() const;

  // Leftover shell applies camera / select / digitize from the same draft.
  TOOL_EXPORT void set_draft_observer(DraftCallback observer);

  // Views hit-test authority. select.* and edit.vertex use this FeatureId.
  // Empty len is a miss. Never synthesize an id from DraftKind.
  using FeatureHit = std::function<content::FeatureId(const Draft&)>;
  TOOL_EXPORT void set_feature_hit(FeatureHit fn);

  // view.full / view.refresh move the camera, then publish the new extent.
  using NavCommand =
      std::function<content::Extent2(std::string_view command_id)>;
  TOOL_EXPORT void set_nav_command(NavCommand fn);

  // Pixel → map CRS for DraftPipeline FeatureGeom commits on draw.*.
  using MapProject =
      std::function<void(int x_px, int y_px, double* map_x, double* map_y)>;
  TOOL_EXPORT void set_map_project(MapProject fn);

  // When true, draw.* skips EditSession (shell / GisScene owns geometry).
  // Prefer false once set_map_project is wired (β FeatureGeom path).
  TOOL_EXPORT void set_shell_owns_append(bool on);

  // Rubber-band geometry for leftover paint. No HWND on this header.
  TOOL_EXPORT void aux_draw();
  TOOL_EXPORT const AuxOverlay* live_preview() const;

  TOOL_EXPORT bool execute(std::string_view command_id, const CommandArgs& args);
  TOOL_EXPORT bool activate(std::string_view interaction_id);
  // 3D exclusive wheel is view3d.*, not always-on wheel.zoom.
  TOOL_EXPORT bool dispatch_input(const content::InputEvent& e);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tool

#endif  // TOOL_WORKSPACE_H_
