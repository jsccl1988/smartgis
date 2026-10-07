// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/core/log.h"
#include "content/browser/session/browser_session.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

void MapPagesComposer::attach_viewports() {
  // Ensure every map tab page has a real client rect before OpenView/Resize
  // (inactive tabs used to keep 0x0 bounds).
  host_->widget_.layout_contents();
  LOGGING(LOG_INFO, "rhi.attach_viewports: layout done; Map Edit attaches now");

  struct Bind {
    ui::views::DrawHost* pane;
    content::ToolSession* host;
    const char* tool;
    bool attach_now;
  };
  // Only DX12-init the visible Map Edit pane at startup. Data + 3D realize
  // HWND only �?three FlyCube devices each busy-waited up to ~5s and made
  // SmartGisViews feel stuck on launch (debug D3D12 layers amplify this).
  // Realize + second layout BEFORE attach so FlyCube Init samples the tab-body
  // client size (not a stale multi-k px rect that leaves a navy-clear present).
  const Bind binds[] = {
      {host_->map_edit_, host_->browser_->edit_tool_session(), "view.pan", true},
      {host_->map_scene_, host_->browser_->scene_tool_session(), "view3d.trackball", false},
  };
  for (const Bind& b : binds) {
    if (!b.pane) {
      continue;
    }
    b.pane->set_tool_session(b.host);
    if (host_->browser_->map_session()) {
      b.pane->set_gis_contents(host_->browser_->map_session());
    }
    if (!b.pane->native_view()) {
      b.pane->realize_native();
    }
    if (!b.attach_now) {
      if (HWND hwnd = b.pane->native_view()) {
        ShowWindow(hwnd, SW_HIDE);
      }
    }
  }
  host_->widget_.layout_contents();
  for (const Bind& b : binds) {
    if (!b.pane || !b.attach_now) {
      continue;
    }
    b.pane->sync_native_bounds();
    b.pane->attach();
    if (b.host) {
      b.host->activate(b.tool);
    }
  }
  host_->for_each_draw_host([](ui::views::DrawHost* pane) {
    if (!pane->native_view()) {
      return;
    }
    pane->sync_native_bounds();
    // Size-notify only panes that already own a present device; deferred
    // Data/3D attach on first tab focus.
    if (pane->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      return;
    }
    RECT rc = {};
    GetClientRect(pane->native_view(), &rc);
    if (rc.right > 0 && rc.bottom > 0) {
      SendMessageW(pane->native_view(), WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(rc.right, rc.bottom));
    }
  });
  // Prefetch leftover GL stereo only when Stereo/GL is the selected engine.
  // Attaching stereo under FlyCube races the DX12 HWND and has corrupted heaps.
  // Scene3d FlyCube itself is deferred until the 3D tab is focused.
  if (host_->map_scene_ && host_->map_scene_->native_view() &&
      host_->map_scene_->attach_mode() != ui::views::DrawHost::AttachMode::kNone &&
      content::BrowserSession::prefers_scene3d_stereo_gl()) {
    (void)host_->browser_->session().try_attach_scene3d_stereo(
        host_->map_scene_->native_view());
  }
}


}  // namespace app