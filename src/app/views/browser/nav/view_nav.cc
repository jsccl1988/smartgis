// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
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

// View menu / map context navigation commands.

void Browser::identify_at(int view_x, int view_y) {
  double map_x = 0;
  double map_y = 0;
  session_->view_to_map(view_x, view_y, &map_x, &map_y);
  const double scale = session_->view_scale() > 1e-9 ? session_->view_scale() : 1.0;
  const double tol_map = 12.0 / scale;
  content::FeatureId saved{};
  bool had = false;
  content::FeatureId id{};
  std::vector<ui::views::FeatureInfo::Hit> hits;
  bool any = false;
  {
    // Copy feature fields before dropping the lock. hit_test_all's pointers
    // alias LayerStore storage that present/ingest may reallocate.
    std::lock_guard<std::recursive_mutex> lock(session_->document().layer_mutex());
    if (const content::GisScene::Feature* current =
            session_->document().selected_feature()) {
      saved = current->id;
      had = true;
    }
    const std::vector<const content::GisScene::Feature*> candidates =
        session_->document().hit_test_all(map_x, map_y, tol_map);
    if (candidates.empty()) {
      if (had) {
        session_->document().select_feature(saved);
      }
    } else {
      any = true;
      id = candidates.front()->id;
      const double map_scale = session_->view_scale();
      hits.reserve(candidates.size());
      for (const content::GisScene::Feature* feature : candidates) {
        if (!feature) {
          continue;
        }
        ui::views::FeatureInfo::Hit hit;
        hit.feature_id = content::GisScene::feature_token(feature->id);
        for (const content::GisScene::Layer& layer : session_->document().layers()) {
          for (const content::GisScene::Feature& candidate : layer.features) {
            if (std::memcmp(candidate.id.bytes, feature->id.bytes,
                            sizeof(feature->id.bytes)) == 0 &&
                candidate.id.len == feature->id.len) {
              hit.layer_name = layer.name;
              break;
            }
          }
          if (!hit.layer_name.empty()) {
            break;
          }
        }
        switch (feature->kind) {
          case content::GisScene::GeomKind::kLine:
            hit.geometry_type = "Line";
            break;
          case content::GisScene::GeomKind::kPolygon:
            hit.geometry_type = "Polygon";
            break;
          case content::GisScene::GeomKind::kText:
            hit.geometry_type = "Text";
            break;
          case content::GisScene::GeomKind::kPoint:
          default:
            hit.geometry_type = "Point";
            break;
        }
        std::vector<std::pair<std::string, std::string>> pairs;
        session_->document().fill_feature_info_fields(*feature, &pairs,
                                                     hit.layer_name, map_scale);
        hit.fields.reserve(pairs.size());
        for (auto& pair : pairs) {
          hit.fields.push_back(
              {std::move(pair.first), std::move(pair.second)});
        }
        hits.push_back(std::move(hit));
      }
    }
  }
  if (!any) {
    session_->navigation_note_no_feature();
    ui_->set_status_message(session_->navigation_status());
    return;
  }
  ui_->sync_inspectors_from_scene();
  const size_t hit_count = hits.size();
  if (ui::views::FeatureInfo* info = ui_->feature_info()) {
    info->set_hits(std::move(hits), 0);
  }
  ui_->show_feature_info_tab();
  ui_->invalidate_map_overlays();
  if (hit_count > 1) {
    ui_->set_status_message("Identified " + std::to_string(hit_count) +
                            " features (" + content::GisScene::feature_token(id) + ")");
  } else {
    ui_->set_status_message("Selected " + content::GisScene::feature_token(id));
  }
}

void Browser::on_view_command(std::string_view command_id,
                                 int bookmark_index, bool from_context,
                                 int view_x, int view_y) {
  if (from_context &&
      (command_id == "view.zoom_in" || command_id == "view.zoom_out")) {
    const double factor = command_id == "view.zoom_in" ? 1.25 : 0.8;
    zoom_at_and_commit(view_x, view_y, factor);
    return;
  }
  if (ui_->scene3d_tab_active() &&
      (command_id == "view.pan" || command_id == "view.zoom_in" ||
       command_id == "view.zoom_out")) {
    // 2D pan/zoom would rewrite the shared extent and leave the orbit on a
    // clipped slice. Browse is the trackball (left-drag pan); zoom dollies.
    if (command_id == "view.pan") {
      run_tool_command("view3d.trackball");
      ui_->set_status_message("3D browse");
      return;
    }
    tool::Draft zoom;
    zoom.kind = tool::DraftKind::kWheel;
    zoom.wheel = command_id == "view.zoom_in" ? 120 : -120;
    session_->scene3d_apply_draft(zoom);
    if (ui_->scene_draw_host()) {
      ui_->invalidate_native_scene();
    }
    ui_->set_status_message(command_id == "view.zoom_in" ? "Zoom in"
                                                          : "Zoom out");
    return;
  }
  if (dispatch_shell_navigation(command_id, bookmark_index, from_context,
                                view_x, view_y)) {
    return;
  }
  run_tool_command(command_id);
}

bool Browser::dispatch_shell_navigation(std::string_view command_id,
                                           int bookmark_index,
                                           bool from_context, int view_x,
                                           int view_y) {
  if (command_id == "view.zoom_layer") {
    content::Extent2 box{};
    const content::Extent2* target =
        session_->document_active_layer_world_extent(&box) ? &box : nullptr;
    if (!session_->navigation_zoom_layer(target)) {
      ui_->set_status_message(session_->navigation_status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Zoom to layer");
    return true;
  }
  if (command_id == "view.zoom_selection") {
    content::Extent2 box{};
    const content::Extent2* target =
        session_->document_selection_world_extent(&box) ? &box : nullptr;
    if (!session_->navigation_zoom_selection(target)) {
      ui_->set_status_message(session_->navigation_status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Zoom to selection");
    return true;
  }
  if (command_id == "view.extent_prev") {
    if (!session_->navigation_previous()) {
      ui_->set_status_message(session_->navigation_status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Previous extent");
    return true;
  }
  if (command_id == "view.extent_next") {
    if (!session_->navigation_next()) {
      ui_->set_status_message(session_->navigation_status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Next extent");
    return true;
  }
  if (command_id == "view.identify") {
    if (from_context) {
      identify_at(view_x, view_y);
    } else {
      run_tool_command("selection.point");
    }
    return true;
  }
  if (command_id == "view.bookmark_add") {
    int w = 800;
    int h = 600;
    ui_->active_view_size(&w, &h);
    const content::Extent2 live = session_->view_world_extent(w, h);
    if (navigation_baselined_) {
      session_->navigation_commit(live);
    } else {
      session_->navigation_reset(live);
    }
    session_->navigation_add_bookmark();
    ui_->schedule_menu_rebuild();
    if (!session_->navigation_bookmarks().empty()) {
      ui_->set_status_message(session_->navigation_bookmarks().back().label);
    }
    return true;
  }
  if (command_id == "view.bookmark_go") {
    if (bookmark_index < 0 ||
        !session_->navigation_go_bookmark(static_cast<size_t>(bookmark_index))) {
      return true;
    }
    frame_navigation_extent();
    const auto index = static_cast<size_t>(bookmark_index);
    if (index < session_->navigation_bookmarks().size()) {
      ui_->set_status_message(session_->navigation_bookmarks()[index].label);
    }
    return true;
  }
  return false;
}

}  // namespace app
