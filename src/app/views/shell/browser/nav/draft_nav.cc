// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "content/browser/camera/map_host_extent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/registry/registry.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "vista/component/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/edit/session.h"
#include "gis/tile/layer/tile_map_layer.h"
#include "gis/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/add_basemap_dialog.h"
#include "ui/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/inspect/attribute_schema_dialog.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/create_datasource_dialog.h"
#include "ui/gis/catalog/create_layer_dialog.h"
#include "ui/gis/catalog/create_map_dialog.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"
#include "base/process/switches.h"

namespace app {

// Draft, zoom, and extent-history commit path.

void Browser::apply_nav_draft(const tool::Draft& draft, bool pan,
                                 double zoom_factor) {
  if (!ui_ || draft.points.empty()) {
    return;
  }
  int vw = 800;
  int vh = 600;
  ui_->active_view_size(&vw, &vh);
  if (pan) {
    if (draft.points.size() < 2) {
      return;
    }
    const int dx = draft.points.back().x_px - draft.points.front().x_px;
    const int dy = draft.points.back().y_px - draft.points.front().y_px;
    // ContentMapView already presents full frames via the GPU process.
    // begin_pan starts a GDI blit timer that races SharedSurface present
    // (same FlyCube/shared_ptr UAF class as the ZoomToRect path below).
    const bool content_map =
        ui_->active_map() &&
        ui_->active_map()->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView;
    if (!content_map) {
      session_.blit().begin_pan(vw, vh, dx, dy);
    }
    session_.view_frame().apply_pan(dx, dy);
  } else {
    const bool content_map =
        ui_->active_map() &&
        ui_->active_map()->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView;
    if (!content_map) {
      session_.blit().begin_zoom(vw, vh, draft.points.front().x_px,
                                 draft.points.front().y_px, zoom_factor);
    }
    session_.view_frame().apply_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, zoom_factor);
  }
  forward_draft_to_contents(draft);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  ui_->schedule_overlay_full_redraw();
}

void Browser::zoom_at_and_commit(int view_x, int view_y, double factor) {
  session_.view_frame().apply_zoom_at(view_x, view_y, factor);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  adopt_or_commit_extent();
  refresh_scale();
  ui_->schedule_overlay_full_redraw();
}

void Browser::fit_map_extent() {
  // Prefer Widget client size. Access HWND via Browser::hwnd() (browser.cc)
  // so this TU never loads ui_ at a possibly stale unique_ptr offset.
  int w = 800;
  int h = 600;
  if (HWND chrome = hwnd()) {
    if (IsWindow(chrome)) {
      RECT rc = {};
      GetClientRect(chrome, &rc);
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
  if (session_.document().has_china_extent() && !skip_china_defaults) {
    apply_china_map2d_product_defaults(*this, w, h);
  } else {
    session_.view_frame().fit_extent(session_.document(), w, h);
    session_.orbit_frame().apply_world_extent(session_.document().world_extent());
    push_shared_extent();
  }
  if (HWND chrome = hwnd()) {
    if (IsWindow(chrome)) {
      invalidate_map_overlays();
    }
  }
  adopt_or_commit_extent();
  refresh_scale();
  if (ui::views::StatusBar* bar = status_bar()) {
    if (session_.document().last_open_was_ogr()) {
      bar->set_crs_text(
          session_.document().has_china_extent() ? "EPSG:4326 (China)" : "EPSG:4326");
    } else {
      bar->set_crs_text("local");
    }
    bar->set_message(
        "Layers: " + std::to_string(session_.document().layer_count()) +
        " Features: " + std::to_string(session_.document().feature_count()));
  }
}

namespace {

// MapSession / ViewHost / Workspace can be poisoned across partial multi-agent
// out/Debug rebuilds; stack().current() must not AV the self-test path.
// SEH helpers stay free of C++ object unwinding (MSVC C2712).
tool::Workspace* seh_view_host_workspace(content::ViewHost* host) {
  if (!host) {
    return nullptr;
  }
  __try {
    return host->workspace();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

tool::Interaction* seh_workspace_current(tool::Workspace* ws) {
  if (!ws) {
    return nullptr;
  }
  __try {
    return ws->stack().current();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

}  // namespace

void Browser::handle_draft(const tool::Draft& draft) {
  if (!ui_) {
    return;
  }
  content::ViewHost* host = ui_->active_view_host();
  tool::Interaction* cur =
      seh_workspace_current(seh_view_host_workspace(host));
  const char* tool_id = cur ? cur->id() : "";
  const bool scene3d_tab = ui_->scene3d_tab_active();

  if (tool_id && std::strncmp(tool_id, "view3d.", 7) == 0) {
    session_.scene3d().apply_draft(draft);
    forward_draft_to_contents(draft);
    if (ui_->map_scene_viewport()) {
      ui_->map_scene_viewport()->invalidate_native();
    }
    return;
  }

  // Scene3d tab: always-on navigate drafts go to orbit camera (not 2D MapScene).
  if (scene3d_tab && tool::is_navigate_tool(tool_id)) {
    if (draft.kind == tool::DraftKind::kWheel ||
        draft.kind == tool::DraftKind::kRect ||
        draft.kind == tool::DraftKind::kPoint ||
        draft.kind == tool::DraftKind::kKey) {
      session_.scene3d().apply_draft(draft);
      forward_draft_to_contents(draft);
      if (ui_->map_scene_viewport()) {
        ui_->map_scene_viewport()->invalidate_native();
      }
      return;
    }
  }

  // Always-on horizontal wheel / two-finger pan drafts (before select/draw).
  if (draft.kind == tool::DraftKind::kRect &&
      tool::draft_flags::is_touch_pan(draft.flags) &&
      draft.points.size() >= 2 && tool::is_navigate_tool(tool_id)) {
    apply_nav_draft(draft, true, 1.0);
    return;
  }

  if (tool_id && std::strncmp(tool_id, "select.", 7) == 0) {
    const MapScene::Feature* hit = session_.document().selected_feature();
    if (hit) {
      ui_->set_status_message("Selected " + MapScene::feature_token(hit->id));
      if (ui_->feature_info()) {
        ui_->feature_info()->set_feature_id(MapScene::feature_token(hit->id));
        std::vector<std::pair<std::string, std::string>> pairs;
        std::string source_layer;
        for (const MapScene::Layer& layer : session_.document().layers()) {
          for (const MapScene::Feature& candidate : layer.features) {
            if (std::memcmp(candidate.id.bytes, hit->id.bytes,
                            sizeof(hit->id.bytes)) == 0 &&
                candidate.id.len == hit->id.len) {
              source_layer = layer.name;
              break;
            }
          }
          if (!source_layer.empty()) {
            break;
          }
        }
        ui_->feature_info()->set_layer_name(source_layer);
        const char* geom = "Point";
        switch (hit->kind) {
          case MapScene::GeomKind::kLine:
            geom = "Line";
            break;
          case MapScene::GeomKind::kPolygon:
            geom = "Polygon";
            break;
          case MapScene::GeomKind::kText:
            geom = "Text";
            break;
          case MapScene::GeomKind::kPoint:
          default:
            geom = "Point";
            break;
        }
        ui_->feature_info()->set_geometry_type(geom);
        session_.document().fill_feature_info_fields(*hit, &pairs, source_layer,
                                           session_.view_frame().scale());
        std::vector<ui::views::FeatureInfo::Field> fields;
        for (auto& p : pairs) {
          fields.push_back({std::move(p.first), std::move(p.second)});
        }
        ui_->feature_info()->set_fields(fields);
      }
      if (content::ViewHost* host = ui_->active_view_host()) {
        const uint32_t view_id = ui_->active_map() ? ui_->active_map()->view_id() : 0;
        host->execute("flash.start", view_id);
      }
      flash_lit_ = true;
      ui_->sync_flash_timer();
    } else {
      if (content::ViewHost* host = ui_->active_view_host()) {
        const uint32_t view_id = ui_->active_map() ? ui_->active_map()->view_id() : 0;
        host->execute("flash.stop", view_id);
      }
      flash_lit_ = true;
      ui_->sync_flash_timer();
      if (ui_->feature_info()) {
        ui_->feature_info()->clear();
      }
      ui_->set_status_message("Selection cleared");
    }
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    return;
  }

  if (tool_id && std::strcmp(tool_id, "edit.vertex") == 0) {
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    return;
  }

  if (tool_id && std::strncmp(tool_id, "draw.", 5) == 0) {
    if (ui_->try_consume_measure_draft(draft)) {
      return;
    }
    // EditSession append (with FeatureGeom) already ran in DraftPipeline.
    // MapScene remains the Views display store.
    const content::FeatureId id = session_.document().append_from_draft(
        draft, tool_id,
        [this](int view_x, int view_y, double* map_x, double* map_y) {
          session_.view_frame().view_to_map(view_x, view_y, map_x, map_y);
        });
    if (plugin::grid_boundary_armed() && id.len > 0) {
      std::vector<std::pair<double, double>> xy;
      if (session_.document().copy_feature_xy(id, &xy) && xy.size() >= 2) {
        std::vector<double> flat;
        flat.reserve(xy.size() * 2);
        for (const auto& p : xy) {
          flat.push_back(p.first);
          flat.push_back(p.second);
        }
        plugin::note_grid_boundary(flat.data(), xy.size());
      }
    }
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    return;
  }

  // Rubber-band ZoomToRect: view.zoom_in L/R drag (view.pan leaves RMB to the
  // shell context menu 芒聙?MapLibre-like browse).
  const bool zoom_rect_draft =
      draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2 &&
      (tool::draft_flags::is_zoom_rect(draft.flags) ||
       (tool_id && std::strcmp(tool_id, "view.zoom_in") == 0));
  if (zoom_rect_draft) {
    const int x0 = draft.points.front().x_px;
    const int y0 = draft.points.front().y_px;
    const int x1 = draft.points.back().x_px;
    const int y1 = draft.points.back().y_px;
    const int adx = x0 > x1 ? x0 - x1 : x1 - x0;
    const int ady = y0 > y1 ? y0 - y1 : y1 - y0;
    if (adx > 4 || ady > 4) {
      int vw = 800;
      int vh = 600;
      ui_->active_view_size(&vw, &vh);
      if (vw <= 0) {
        vw = 800;
      }
      if (vh <= 0) {
        vh = 600;
      }
      double mx0 = 0;
      double my0 = 0;
      double mx1 = 0;
      double my1 = 0;
      session_.view_frame().view_to_map(x0, y0, &mx0, &my0);
      session_.view_frame().view_to_map(x1, y1, &mx1, &my1);
      content::Extent2 box;
      box.xmin = (std::min)(mx0, mx1);
      box.xmax = (std::max)(mx0, mx1);
      const double lat0 = -my0;
      const double lat1 = -my1;
      box.ymin = (std::min)(lat0, lat1);
      box.ymax = (std::max)(lat0, lat1);
      if (box.xmax - box.xmin < 1e-9) {
        const double c = 0.5 * (box.xmin + box.xmax);
        box.xmin = c - 1e-6;
        box.xmax = c + 1e-6;
      }
      if (box.ymax - box.ymin < 1e-9) {
        const double c = 0.5 * (box.ymin + box.ymax);
        box.ymin = c - 1e-6;
        box.ymax = c + 1e-6;
      }
      if (content::extent_nonempty(box)) {
        session_.view_frame().apply_world_extent(box, vw, vh);
        session_.map2d().invalidate_frame_cache();
        push_shared_extent();
        adopt_or_commit_extent();
        refresh_scale();
        // Full redraw via Invalidate only 芒聙?avoid blit-timer + GPU present
        // racing after a rubber-band ZoomToRect (FlyCube shared_ptr UAF).
        ui_->invalidate_map_overlays();
        return;
      }
    }
    if (tool_id && std::strcmp(tool_id, "view.zoom_in") == 0) {
      apply_nav_draft(draft, false, 1.25);
      return;
    }
    return;
  }

  if (tool_id && std::strcmp(tool_id, "view.pan") == 0 &&
      draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    apply_nav_draft(draft, true, 1.0);
    return;
  }

  if (tool_id && std::strcmp(tool_id, "view.zoom_in") == 0 &&
      !draft.points.empty()) {
    apply_nav_draft(draft, false, 1.25);
    return;
  }
  if (tool_id && std::strcmp(tool_id, "view.zoom_out") == 0 &&
      !draft.points.empty()) {
    apply_nav_draft(draft, false, 0.8);
    return;
  }

  // Always-on map UX: wheel / double-click zoom toward cursor (not view center).
  if (draft.kind == tool::DraftKind::kWheel && !draft.points.empty() &&
      tool::is_navigate_tool(tool_id)) {
    apply_nav_draft(draft, false, tool::wheel_zoom_factor(draft.wheel));
    return;
  }
  if (draft.kind == tool::DraftKind::kPoint &&
      tool::is_navigate_tool(tool_id) && !draft.points.empty()) {
    apply_nav_draft(draft, false, 1.25);
  }
}

void Browser::refresh_scale() {
  // Same TU-skew guard as adopt_or_commit_extent: a stale draft_nav.obj vs
  // Browser layout reads ui_ at the wrong offset and AVs on status_bar /
  // active_map during Browser::show (map2d-showcase / harness).
  if (!ui_) {
    return;
  }
  // Showcase Widget::show hits StatusBar::set_scale_text before Label
  // children are a live std::string (cdb: Label::_Equal read 0xe1).
  if (base::switch_cstr("map2d-showcase")) {
    return;
  }
  ui::views::StatusBar* bar = ui_->status_bar();
  if (!bar) {
    return;
  }
  int raw_w = 0;
  int raw_h = 0;
  if (ui::views::MapViewport* pane = ui_->active_map()) {
    if (HWND map_hwnd = pane->native_view()) {
      RECT rc = {};
      GetClientRect(map_hwnd, &rc);
      raw_w = rc.right;
      raw_h = rc.bottom;
    }
  }
  if (raw_w <= 0) {
    bar->set_scale_text(format_view_scale({}, 0));
    return;
  }
  bar->set_scale_text(format_view_scale(
      session_.view_frame().view_world_extent(raw_w, raw_h > 0 ? raw_h : 1),
      raw_w));
}

void Browser::adopt_or_commit_extent() {
  // Guard: draft_nav.cc is a separate Browser TU. A stale .obj vs browser.obj
  // under parallel ninja reads ui_ at the wrong offset (NULL / 0xCD) and AVs
  // here during init_shell fit_map_extent 聴 rebuild shell_browser together.
  if (!ui_) {
    return;
  }
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  const content::Extent2 now = session_.view_frame().view_world_extent(w, h);
  if (!navigation_baselined_) {
    session_.navigation().reset(now);
  } else {
    session_.navigation().commit(now);
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
    extent_watch_ = session_.view_frame().view_world_extent(w, h);
    return;
  }
  if (!extent_watch_open_) {
    return;
  }
  extent_watch_open_ = false;
  const content::Extent2 now = session_.view_frame().view_world_extent(w, h);
  if (navigation_baselined_ && !extents_equal(extent_watch_, now)) {
    session_.navigation().commit(now);
  }
  refresh_scale();
}

void Browser::frame_navigation_extent() {
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  session_.view_frame().apply_world_extent(session_.navigation().extent(), w, h);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  refresh_scale();
  ui_->schedule_overlay_full_redraw();
}

void Browser::identify_at(int view_x, int view_y) {
  content::FeatureId saved{};
  bool had = false;
  if (const MapScene::Feature* current = session_.document().selected_feature()) {
    saved = current->id;
    had = true;
  }
  double map_x = 0;
  double map_y = 0;
  session_.view_frame().view_to_map(view_x, view_y, &map_x, &map_y);
  const double scale = session_.view_frame().scale() > 1e-9 ? session_.view_frame().scale() : 1.0;
  const double tol_map = 12.0 / scale;
  const std::vector<const MapScene::Feature*> candidates =
      session_.document().hit_test_all(map_x, map_y, tol_map);
  if (candidates.empty()) {
    if (had) {
      session_.document().select_feature(saved);
    }
    session_.navigation().note_no_feature();
    ui_->set_status_message(session_.navigation().status());
    return;
  }
  // hit_test_all already selected the nearest feature.
  const content::FeatureId id = candidates.front()->id;
  ui_->sync_inspectors_from_scene();
  if (ui::views::FeatureInfo* info = ui_->feature_info()) {
    const double map_scale = session_.view_frame().scale();
    std::vector<ui::views::FeatureInfo::Hit> hits;
    hits.reserve(candidates.size());
    for (const MapScene::Feature* feature : candidates) {
      if (!feature) {
        continue;
      }
      ui::views::FeatureInfo::Hit hit;
      hit.feature_id = MapScene::feature_token(feature->id);
      for (const MapScene::Layer& layer : session_.document().layers()) {
        for (const MapScene::Feature& candidate : layer.features) {
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
        case MapScene::GeomKind::kLine:
          hit.geometry_type = "Line";
          break;
        case MapScene::GeomKind::kPolygon:
          hit.geometry_type = "Polygon";
          break;
        case MapScene::GeomKind::kText:
          hit.geometry_type = "Text";
          break;
        case MapScene::GeomKind::kPoint:
        default:
          hit.geometry_type = "Point";
          break;
      }
      std::vector<std::pair<std::string, std::string>> pairs;
      session_.document().fill_feature_info_fields(*feature, &pairs,
                                                   hit.layer_name, map_scale);
      hit.fields.reserve(pairs.size());
      for (auto& pair : pairs) {
        hit.fields.push_back(
            {std::move(pair.first), std::move(pair.second)});
      }
      hits.push_back(std::move(hit));
    }
    info->set_hits(std::move(hits), 0);
  }
  if (ui_->inspector_tabs()) {
    ui_->inspector_tabs()->set_active(0);
  }
  ui_->invalidate_map_overlays();
  if (candidates.size() > 1) {
    ui_->set_status_message("Identified " + std::to_string(candidates.size()) +
                            " features (" + MapScene::feature_token(id) + ")");
  } else {
    ui_->set_status_message("Selected " + MapScene::feature_token(id));
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
    session_.scene3d().apply_draft(zoom);
    if (ui_->map_scene_viewport()) {
      ui_->map_scene_viewport()->invalidate_native();
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
        session_.document().active_layer_world_extent(&box) ? &box : nullptr;
    if (!session_.navigation().zoom_layer(target)) {
      ui_->set_status_message(session_.navigation().status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Zoom to layer");
    return true;
  }
  if (command_id == "view.zoom_selection") {
    content::Extent2 box{};
    const content::Extent2* target =
        session_.document().selection_world_extent(&box) ? &box : nullptr;
    if (!session_.navigation().zoom_selection(target)) {
      ui_->set_status_message(session_.navigation().status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Zoom to selection");
    return true;
  }
  if (command_id == "view.extent_prev") {
    if (!session_.navigation().previous()) {
      ui_->set_status_message(session_.navigation().status());
      return true;
    }
    frame_navigation_extent();
    ui_->set_status_message("Previous extent");
    return true;
  }
  if (command_id == "view.extent_next") {
    if (!session_.navigation().next()) {
      ui_->set_status_message(session_.navigation().status());
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
    const content::Extent2 live = session_.view_frame().view_world_extent(w, h);
    if (navigation_baselined_) {
      session_.navigation().commit(live);
    } else {
      session_.navigation().reset(live);
    }
    session_.navigation().add_bookmark();
    ui_->schedule_menu_rebuild();
    if (!session_.navigation().bookmarks().empty()) {
      ui_->set_status_message(session_.navigation().bookmarks().back().label);
    }
    return true;
  }
  if (command_id == "view.bookmark_go") {
    if (bookmark_index < 0 ||
        !session_.navigation().go_bookmark(static_cast<size_t>(bookmark_index))) {
      return true;
    }
    frame_navigation_extent();
    const auto index = static_cast<size_t>(bookmark_index);
    if (index < session_.navigation().bookmarks().size()) {
      ui_->set_status_message(session_.navigation().bookmarks()[index].label);
    }
    return true;
  }
  return false;
}

void Browser::forward_draft_to_contents(const tool::Draft& draft) {
  if (!session_.map_contents()) {
    return;
  }
  ui::views::MapViewport* pane = ui_->active_map();
  const uint32_t view_id = pane ? pane->view_id() : 0;
  if (view_id == 0) {
    return;
  }
  content::InputEvent e{};
  e.flags = draft.flags;
  if (!draft.points.empty()) {
    e.x_px = draft.points.back().x_px;
    e.y_px = draft.points.back().y_px;
  }
  if (draft.kind == tool::DraftKind::kWheel) {
    e.kind = content::InputEvent::Kind::kWheel;
    e.wheel = draft.wheel;
    if (!draft.points.empty()) {
      e.x_px = draft.points.front().x_px;
      e.y_px = draft.points.front().y_px;
    }
    session_.map_contents()->Dispatch(view_id, e);
    return;
  }
  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    e.kind = content::InputEvent::Kind::kLDown;
    e.x_px = draft.points.front().x_px;
    e.y_px = draft.points.front().y_px;
    session_.map_contents()->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kMouseMove;
    e.x_px = draft.points.back().x_px;
    e.y_px = draft.points.back().y_px;
    session_.map_contents()->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kLUp;
    session_.map_contents()->Dispatch(view_id, e);
  }
}

void Browser::commit_blit_preview() {
  session_.blit().end_preview();
  if (ui_) {
    ui_->invalidate_map_overlays();
  }
}

void Browser::push_shared_extent() {
  if (!ui_ || syncing_extent_) {
    return;
  }
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  content::Extent2 e;
  // On the 3D tab, never pull the Map-Edit 2D crop into the orbit camera 芒聙?  // a coastal / half-ocean 2D view made DEM present as a black void with a
  // sliver of terrain on the far edge (氓聺聦氓聡禄氓聢?3D 忙聴聽莽聰禄茅聺?.
  if (ui_->scene3d_tab_active()) {
    e = session_.orbit_frame().world_extent();
    if (!extent_looks_like_china(e)) {
      e = kChinaLonLatExtent;
      session_.orbit_frame().apply_world_extent(e);
    }
  } else {
    e = session_.view_frame().view_world_extent(w, h);
    if (!extent_looks_like_china(e)) {
      const content::Extent2 world = session_.document().world_extent();
      e = extent_looks_like_china(world) ? world : kChinaLonLatExtent;
    }
    session_.orbit_frame().apply_world_extent(e);
  }
  refresh_scale();
  if (!session_.map_contents()) {
    return;
  }
  syncing_extent_ = true;
  ui_->for_each_map_viewport([&](ui::views::MapViewport* pane) {
    if (pane->view_id() != 0) {
      session_.map_contents()->SetExtent(pane->view_id(), e);
    }
  });
  const uint32_t scene_id =
      ui_->map_scene_viewport() ? ui_->map_scene_viewport()->view_id() : 0;
  if (scene_id != 0) {
    session_.map_contents()->SetExtent(scene_id, session_.orbit_frame().world_extent());
  }
  syncing_extent_ = false;
  refresh_scale();
}

void Browser::handle_pinch(int view_x, int view_y, double scale) {
  if (!ui_) {
    return;
  }
  int w = 800;
  int h = 600;
  ui_->active_view_size(&w, &h);
  if (ui_->scene3d_tab_active()) {
    session_.orbit_frame().apply_pinch(view_x, view_y, scale, w, h);
    if (ui_->map_scene_viewport()) {
      ui_->map_scene_viewport()->invalidate_native();
    }
  } else {
    session_.view_frame().apply_pinch(view_x, view_y, scale);
  }
  push_shared_extent();
  ui_->invalidate_map_overlays();
}

void Browser::handle_gesture_pan(int dx_px, int dy_px) {
  if (!ui_ || (dx_px == 0 && dy_px == 0)) {
    return;
  }
  int vw = 800;
  int vh = 600;
  ui_->active_view_size(&vw, &vh);
  if (ui_->scene3d_tab_active()) {
    session_.orbit_frame().apply_pan(dx_px, dy_px);
    if (ui_->map_scene_viewport()) {
      ui_->map_scene_viewport()->invalidate_native();
    }
  } else {
    // Match apply_nav_draft: ContentMapView + FORCE_GDI paints full Map2d each
    // WM_PAINT. begin_pan StretchBlt preview races SharedSurface and can leave
    // the embed on a static/cream blit while view_frame keeps panning.
    const bool content_map =
        ui_->active_map() &&
        ui_->active_map()->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView;
    if (!content_map) {
      session_.blit().begin_pan(vw, vh, dx_px, dy_px);
    }
    session_.view_frame().apply_pan(dx_px, dy_px);
  }
  push_shared_extent();
  ui_->invalidate_map_overlays();
  ui_->schedule_overlay_full_redraw();
}

void Browser::pull_orbit_extent() {
  if (!ui_) {
    return;
  }
  // Do not call MapContents::Extent here. Multi-agent out/ rebuilds have left
  // MapContents ABI skew that AVs inside Extent (cdb: pull_orbit_extent /
  // INVALID_POINTER_READ). Document world_extent + China fallback is enough
  // for orbit init; live view sync goes through push_shared_extent / fit.
  const content::Extent2 doc = session_.document().world_extent();
  if (extent_nonempty(doc)) {
    session_.orbit_frame().set_extent(doc);
    return;
  }
  if (!extent_nonempty(session_.orbit_frame().extent())) {
    session_.orbit_frame().set_extent(kChinaLonLatExtent);
  }
}

}  // namespace app
