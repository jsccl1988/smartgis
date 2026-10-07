// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <string_view>

#include "app/views/ui/pages/detail/ptr_guard.h"
#include "app/views/ui/pages/detail/seh_workspace.h"
#include "base/core/log.h"
#include "content/browser/session/browser_session.h"
#include "content/public/view_host.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"

namespace app {

void MapPagesComposer::wire_tool_seams() {
  // Snapshot owned host pointers once. Do not iterate a temporary list that
  // re-reads session getters after map2d/scene3d bind �?a skewed BrowserSession
  // layout can poison trailing unique_ptrs mid-init_shell.
  content::ViewHost* const hosts[3] = {
      host_->browser_->edit_host(), host_->browser_->data_host(), host_->browser_->scene_host()};

  auto resolve = [this](const tool::Draft& draft) -> content::FeatureId {
    content::ViewHost* host = host_->active_view_host();
    if (!host) {
      return {};
    }
    tool::Workspace* ws = host->workspace();
    tool::Interaction* cur = ws ? ws->stack().current() : nullptr;
    const char* tool_id = cur ? cur->id() : "";
    if (!tool_id || draft.points.empty()) {
      return {};
    }
    const int px = draft.points.front().x_px;
    const int py = draft.points.front().y_px;
    double map_x = 0;
    double map_y = 0;
    host_->browser_->session().view_to_map(px, py, &map_x, &map_y);
    const double scale = host_->browser_->session().view_scale() > 1e-9
                             ? host_->browser_->session().view_scale()
                             : 1.0;
    const double tol_map = 12.0 / scale;
    if (std::strncmp(tool_id, "select.", 7) == 0) {
      return host_->browser_->session().select_feature_at(map_x, map_y, tol_map);
    }
    if (std::strcmp(tool_id, "edit.vertex") == 0) {
      return host_->browser_->session().move_selected_vertex(map_x, map_y, tol_map);
    }
    return {};
  };
  auto nav = [this](std::string_view command_id) -> content::Extent2 {
    int w = 800;
    int h = 600;
    host_->active_view_size(&w, &h);
    if (command_id == "view.full" || command_id == "view3d.full") {
      host_->browser_->session().frame_view_and_orbit_to_document(w, h);
      host_->browser_->push_shared_extent();
      host_->invalidate_map_overlays();
    } else if (command_id == "view.refresh") {
      host_->browser_->session().reapply_view_world_extent(w, h);
      host_->browser_->push_shared_extent();
      host_->invalidate_map_overlays();
    }
    return host_->browser_->session().view_world_extent(w, h);
  };
  auto on_draft = [this](const tool::Draft& draft) {
    host_->browser_->handle_draft(draft);
  };
  auto map_project = [this](int x_px, int y_px, double* map_x, double* map_y) {
    host_->browser_->session().view_to_map(x_px, y_px, map_x, map_y);
  };
  // Guard against skewed BrowserSession layouts from parallel out/Debug rebuilds:
  // edit_host_ can be 0xCDCDCDCD / 0xCDCDCD00 and ViewHost::workspace AVs.
  tool::DraftCallback draft_cb = on_draft;
  tool::Workspace::FeatureHit hit_cb = resolve;
  tool::Workspace::NavCommand nav_cb = nav;
  tool::Workspace::MapProject project_cb = map_project;
  detail::WorkspaceBindFns bind_fns{
      &detail::trampoline_set_draft, &detail::trampoline_set_hit, &detail::trampoline_set_nav,
      &detail::trampoline_set_project, &draft_cb,         &hit_cb,
      &nav_cb,               &project_cb};
  for (content::ViewHost* host : hosts) {
    if (!host || detail::ptr_addr_poison(reinterpret_cast<uintptr_t>(host)) ||
        !detail::ptr_mem_readable(host, sizeof(void*))) {
      LOGGING(LOG_WARNING, "wire_tool_seams: skip invalid ViewHost %p", host);
      continue;
    }
    tool::Workspace* ws = detail::seh_view_host_workspace(host);
    if (!ws || detail::ptr_addr_poison(reinterpret_cast<uintptr_t>(ws)) ||
        !detail::ptr_mem_readable(ws, sizeof(void*))) {
      LOGGING(LOG_WARNING, "wire_tool_seams: skip invalid Workspace %p (host=%p)",
              ws, host);
      continue;
    }
    if (!detail::seh_bind_workspace(ws, &bind_fns)) {
      LOGGING(LOG_WARNING,
              "wire_tool_seams: Workspace bind AV host=%p ws=%p (skip)", host,
              ws);
    }
  }
}


}  // namespace app