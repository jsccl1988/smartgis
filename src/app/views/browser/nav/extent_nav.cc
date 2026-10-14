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

#include "app/views/browser/nav/detail/attach_mode.h"
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
namespace {

// ViewNavigation::reset/commit can AV under skewed BrowserSession layout.
bool seh_nav_reset_or_commit(content::BrowserSession* session,
                             const content::Extent2& now,
                             bool baselined) {
  if (!session) {
    return false;
  }
  __try {
    if (!baselined) {
      session->navigation_reset(now);
    } else {
      session->navigation_commit(now);
    }
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace

// Extent fit / history / shared GisContents sync.

void Browser::fit_map_extent() {
  // Prefer Widget client size. Access HWND via Browser::hwnd() (browser.cc)
  // so this TU never loads ui_ at a possibly stale unique_ptr offset.
  int w = 800;
  int h = 600;
  if (HWND horizon = hwnd()) {
    if (IsWindow(horizon)) {
      RECT rc = {};
      GetClientRect(horizon, &rc);
      if (rc.right > 32) {
        w = rc.right;
      }
      if (rc.bottom > 32) {
        h = rc.bottom;
      }
    }
  }
  const bool skip_china_defaults = []() {
    const char* skip = base::switch_cstr("skip-china-map2d-defaults");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  if (session_->document_has_land_surface() && !skip_china_defaults) {
    apply_china_map2d_product_defaults(*this, w, h);
  } else {
    session_->frame_view_and_orbit_to_document(w, h);
    push_shared_extent();
  }
  if (HWND horizon = hwnd()) {
    if (IsWindow(horizon)) {
      invalidate_map_overlays();
    }
  }
  adopt_or_commit_extent();
  refresh_scale();
  if (ui::views::StatusBar* bar = status_bar()) {
    if (session_->document_last_open_was_ogr()) {
      bar->set_crs_text("EPSG:4326");
    } else {
      bar->set_crs_text("local");
    }
    // Compact message -- status cell is leftmost; avoid jammed
    // "Layers: N Features: M".
    bar->set_message(std::format(
        "{} layers 路 {} feats", session_->document_layer_count(),
        session_->document_feature_count()));
  }
}

void Browser::refresh_scale() {
  // Same TU-skew guard as adopt_or_commit_extent: a stale draft_nav.obj vs
  // Browser layout reads ui_ at the wrong offset and AVs on status_bar /
  // active_map during Browser::show (map2d-showcase / harness).
  if (!ui_) {
    return;
  }
  ui::views::StatusBar* bar = ui_->status_bar();
  if (!bar) {
    return;
  }
  int raw_w = 0;
  int raw_h = 0;
  if (ui::views::DrawHost* pane = ui_->active_map()) {
    if (HWND map_hwnd = pane->native_view()) {
      RECT rc = {};
      GetClientRect(map_hwnd, &rc);
      raw_w = rc.right;
      raw_h = rc.bottom;
    }
  }
  if (raw_w <= 0) {
    bar->set_scale_text(content::format_view_scale({}, 0));
    return;
  }
  bar->set_scale_text(content::format_view_scale(
      session_->view_world_extent(raw_w, raw_h > 0 ? raw_h : 1),
      raw_w));
}

void Browser::adopt_or_commit_extent() {
  // Prefer HWND client size (browser.cc hwnd()) so this TU never loads ui_ at
  // a possibly stale unique_ptr offset during init_shell fit_map_extent.
  if (!session_) {
    return;
  }
  int w = 800;
  int h = 600;
  if (HWND horizon = hwnd()) {
    if (IsWindow(horizon)) {
      RECT rc = {};
      GetClientRect(horizon, &rc);
      if (rc.right > 32) {
        w = rc.right;
      }
      if (rc.bottom > 32) {
        h = rc.bottom;
      }
    }
  } else if (ui_) {
    ui_->active_view_size(&w, &h);
  }
  const content::Extent2 now = session_->view_world_extent(w, h);
  if (!seh_nav_reset_or_commit(session_.get(), now, navigation_baselined_)) {
    std::fprintf(stderr, "startup: navigation reset/commit SEH fail\n");
  }
}

void Browser::on_extent_watch(bool begin) {
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  if (begin) {
    if (extent_watch_open_) {
      return;
    }
    extent_watch_open_ = true;
    extent_watch_ = session_->view_world_extent(w, h);
    return;
  }
  if (!extent_watch_open_) {
    return;
  }
  extent_watch_open_ = false;
  const content::Extent2 now = session_->view_world_extent(w, h);
  if (navigation_baselined_ && !content::extents_equal(extent_watch_, now)) {
    session_->navigation_commit(now);
  }
  refresh_scale();
}

void Browser::frame_navigation_extent() {
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  session_->apply_view_world_extent(session_->navigation_extent(), w, h);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  refresh_scale();
  ui_->schedule_overlay_full_redraw();
}

void Browser::push_shared_extent() {
  if (!ui_ || syncing_extent_) {
    return;
  }
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  content::Extent2 e;
  // On the 3D tab, never pull the Map-Edit 2D crop into the orbit camera ---?  // a coastal / half-ocean 2D view made DEM present as a black void with a
  // sliver of terrain on the far edge (------------?3D ------------?.
  if (ui_->scene3d_tab_active()) {
    e = session_->orbit_extent_for_scene_tab();
  } else {
    e = session_->adopt_orbit_from_view(w, h);
  }
  refresh_scale();
  if (!session_->gis_contents()) {
    return;
  }
  syncing_extent_ = true;
  ui_->for_each_draw_host([&](ui::views::DrawHost* pane) {
    if (pane->view_id() != 0) {
      session_->gis_contents()->SetExtent(pane->view_id(), e);
    }
  });
  const uint32_t scene_id = ui_->scene_view_id();
  if (scene_id != 0) {
    session_->gis_contents()->SetExtent(scene_id, session_->orbit_world_extent());
  }
  syncing_extent_ = false;
  refresh_scale();
}

void Browser::pull_orbit_extent() {
  if (!ui_) {
    return;
  }
  // Do not call GisContents::Extent here. Multi-agent out/ rebuilds have left
  // GisContents ABI skew that AVs inside Extent (cdb: pull_orbit_extent /
  // INVALID_POINTER_READ). Document world_extent + China fallback is enough
  // for orbit init; live view sync goes through push_shared_extent / fit.
  session_->pull_orbit_extent_from_document();
}

}  // namespace app
