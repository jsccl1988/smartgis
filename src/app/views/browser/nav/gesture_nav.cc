// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/nav/detail/seh_workspace.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/process/switches.h"
#include "content/browser/camera/view_navigation.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_contents.h"
#include "content/public/tool_session.h"
#include "content/public/types.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

// Pinch / gesture pan / cursor zoom commit.

void Browser::zoom_at_and_commit(int view_x, int view_y, double factor) {
  session_->apply_view_zoom_at(view_x, view_y, factor);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  adopt_or_commit_extent();
  refresh_scale();
  ui_->schedule_overlay_full_redraw();
}

void Browser::handle_pinch(int view_x, int view_y, double scale) {
  if (!ui_) {
    return;
  }
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  if (ui_->scene3d_tab_active()) {
    session_->apply_orbit_pinch(view_x, view_y, scale, w, h);
    if (ui_->scene_draw_host()) {
      ui_->invalidate_native_scene();
    }
  } else {
    session_->apply_view_pinch(view_x, view_y, scale);
  }
  push_shared_extent();
  ui_->invalidate_map_overlays();
}

void Browser::handle_gesture_pan(int dx_px, int dy_px) {
  if (!ui_ || (dx_px == 0 && dy_px == 0)) {
    return;
  }
  // Map2d: pan only via LMB click-drag (view.pan → apply_nav_draft). Ignore
  // WM_GESTURE GID_PAN (two-finger / trackpad left-right) so gestures do not
  // move the map. Scene3d keeps orbit pan from the same HWND path.
  if (!ui_->scene3d_tab_active()) {
    return;
  }
  int vw = 800;
  int vh = 600;
  ui_->active_view_size(&vw, &vh);
  session_->apply_orbit_pan(dx_px, dy_px);
  if (ui_->scene_draw_host()) {
    ui_->invalidate_native_scene();
  }
  push_shared_extent();
  ui_->invalidate_map_overlays();
  ui_->schedule_overlay_full_redraw();
}

}  // namespace app
