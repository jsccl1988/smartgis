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
#include "base/core/log.h"
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

// Draft / tool commit path.

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
    const bool content_map = detail::is_content_mapview(ui_->active_map());
    if (!content_map) {
      session_->blit_begin_pan(vw, vh, dx, dy);
    }
    session_->apply_view_pan(dx, dy);
  } else {
    const bool content_map = detail::is_content_mapview(ui_->active_map());
    if (!content_map) {
      session_->blit_begin_zoom(vw, vh, draft.points.front().x_px,
                                 draft.points.front().y_px, zoom_factor);
    }
    session_->apply_view_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, zoom_factor);
  }
  forward_draft_to_contents(draft);
  push_shared_extent();
  ui_->invalidate_map_overlays();
  ui_->schedule_overlay_full_redraw();
}

void Browser::handle_draft(const tool::Draft& draft) {
  if (!ui_) {
    return;
  }
  content::ToolSession* host = ui_->active_tool_session();
  tool::Interaction* cur =
      detail::seh_workspace_current(detail::seh_tool_session_workspace(host));
  const char* tool_id = cur ? cur->id() : "";
  const bool scene3d_tab = ui_->scene3d_tab_active();

  if (tool_id && std::strncmp(tool_id, "view3d.", 7) == 0) {
    session_->scene3d_apply_draft(draft);
    forward_draft_to_contents(draft);
    if (ui_->scene_draw_host()) {
      ui_->invalidate_native_scene();
    }
    return;
  }

  // Scene3d tab: always-on navigate drafts go to orbit camera (not 2D GisScene).
  if (scene3d_tab && tool::is_navigate_tool(tool_id)) {
    if (draft.kind == tool::DraftKind::kWheel ||
        draft.kind == tool::DraftKind::kRect ||
        draft.kind == tool::DraftKind::kPoint ||
        draft.kind == tool::DraftKind::kKey) {
      session_->scene3d_apply_draft(draft);
      forward_draft_to_contents(draft);
      ui_->invalidate_native_scene();
      return;
    }
  }

  // Map2d: swallow always-on horizontal-wheel / two-finger kTouchPan without
  // applying. Pan only via LMB click-drag on view.pan (below). Scene3d touch
  // pan is handled in the scene3d_tab block above.
  if (draft.kind == tool::DraftKind::kRect &&
      tool::draft_flags::is_touch_pan(draft.flags) &&
      tool::is_navigate_tool(tool_id)) {
    return;
  }

  if (tool_id && std::strncmp(tool_id, "select.", 7) == 0) {
    const content::GisScene::Feature* hit = session_->document().selected_feature();
    if (hit) {
      const std::string token = content::GisScene::feature_token(hit->id);
      LOGGING(LOG_INFO, "map.select %s", token.c_str());
      ui_->set_status_message("Selected " + token);
      if (ui_->feature_info()) {
        ui_->feature_info()->set_feature_id(content::GisScene::feature_token(hit->id));
        std::vector<std::pair<std::string, std::string>> pairs;
        std::string source_layer;
        for (const content::GisScene::Layer& layer : session_->document().layers()) {
          for (const content::GisScene::Feature& candidate : layer.features) {
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
          case content::GisScene::GeomKind::kLine:
            geom = "Line";
            break;
          case content::GisScene::GeomKind::kPolygon:
            geom = "Polygon";
            break;
          case content::GisScene::GeomKind::kText:
            geom = "Text";
            break;
          case content::GisScene::GeomKind::kPoint:
          default:
            geom = "Point";
            break;
        }
        ui_->feature_info()->set_geometry_type(geom);
        session_->document().fill_feature_info_fields(*hit, &pairs, source_layer,
                                           session_->view_scale());
        std::vector<ui::views::FeatureInfo::Field> fields;
        for (auto& p : pairs) {
          fields.push_back({std::move(p.first), std::move(p.second)});
        }
        ui_->feature_info()->set_fields(fields);
      }
      if (content::ToolSession* host = ui_->active_tool_session()) {
        const uint32_t view_id = ui_->active_map() ? ui_->active_map()->view_id() : 0;
        host->execute("flash.start", view_id);
      }
      flash_lit_ = true;
      ui_->sync_flash_timer();
    } else {
      LOGGING(LOG_INFO, "map.select miss");
      if (content::ToolSession* host = ui_->active_tool_session()) {
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
    // GisScene remains the Views display store.
    const content::FeatureId id = session_->append_from_draft(draft, tool_id);
    if (plugin::grid_boundary_armed() && id.len > 0) {
      std::vector<std::pair<double, double>> xy;
      if (session_->copy_feature_xy(id, &xy) && xy.size() >= 2) {
        std::vector<double> flat;
        flat.reserve(xy.size() * 2);
        for (const auto& p : xy) {
          flat.push_back(p.first);
          flat.push_back(p.second);
        }
        plugin::note_grid_boundary(flat.data(), xy.size());
      }
    }
    // Same string as EditCommitted (inspector_sync). Self-test and the
    // status bar must not depend on EventBus wiring surviving tab switch.
    ui_->set_status_message("Committed append");
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    return;
  }

  // Rubber-band ZoomToRect: view.zoom_in L/R drag (view.pan leaves RMB to the
  // shell context menu ---?MapLibre-like browse).
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
      session_->view_to_map(x0, y0, &mx0, &my0);
      session_->view_to_map(x1, y1, &mx1, &my1);
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
      if (content::BrowserSession::is_extent_nonempty(box)) {
        session_->apply_view_world_extent(box, vw, vh);
        // ZoomToRect is camera-only; fingerprint gate keeps layout warm.
        session_->invalidate_map2d_frame_cache();
        push_shared_extent();
        adopt_or_commit_extent();
        refresh_scale();
        // Full redraw via Invalidate only ---?avoid blit-timer + GPU present
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

void Browser::forward_draft_to_contents(const tool::Draft& draft) {
  if (!session_->gis_contents()) {
    return;
  }
  ui::views::DrawHost* pane = ui_->active_map();
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
    session_->gis_contents()->Dispatch(view_id, e);
    return;
  }
  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    e.kind = content::InputEvent::Kind::kLDown;
    e.x_px = draft.points.front().x_px;
    e.y_px = draft.points.front().y_px;
    session_->gis_contents()->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kMouseMove;
    e.x_px = draft.points.back().x_px;
    e.y_px = draft.points.back().y_px;
    session_->gis_contents()->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kLUp;
    session_->gis_contents()->Dispatch(view_id, e);
  }
}

void Browser::commit_blit_preview() {
  session_->blit_end_preview();
  if (ui_) {
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
