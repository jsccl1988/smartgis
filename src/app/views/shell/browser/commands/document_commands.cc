// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {
std::string json_escape(const std::string& text);
void catalog_call(content::MapContents* session, const std::string& json);
std::string path_stem(const std::string& path);
}  // namespace detail

bool Browser::run_tool_command(std::string_view command_id) {
  if (dispatch_shell_navigation(command_id, -1, false, 0, 0)) {
    return true;
  }
  content::ViewHost* host = ui_->active_view_host();
  if (!host || command_id.empty()) {
    return false;
  }
  std::string id(command_id);
  if (id == "select" || id == "identify") {
    id = "selection.point";
  } else if (id == "pan") {
    id = "view.pan";
  } else if (id == "full") {
    id = "view.full";
  }
  const uint32_t view_id = ui_->active_map() ? ui_->active_map()->view_id() : 0;
  if (!host->execute(id, view_id)) {
    ui_->set_status_message("Unknown tool " + id);
    return false;
  }
  if (id == "view.full" || id == "view3d.full") {
    fit_map_extent();
    // 2D fit does not pull the orbit back out of a clipped DEM. Full on the
    // 3D tab restores the framed yaw/pitch/distance.
    if (ui_->scene3d_tab_active()) {
      session_.orbit_frame().reset();
      if (ui_->map_scene_viewport()) {
        ui_->map_scene_viewport()->invalidate_native();
      }
    }
    ui_->set_status_message("View full extent");
    return true;
  }
  if (id == "view.backend.rhi" || id == "view.backend.maplibre") {
    const uint32_t kind = (id == "view.backend.maplibre") ? 1u : 0u;
    if (session_.map_contents()) {
      session_.map_contents()->SetRenderBackend(kind);
    }
    ui_->set_status_message(kind ? "Render: MapLibre (Track A)"
                            : "Render: RHI (Track B)");
    return true;
  }
  if (id == "view.engine.flycube" || id == "view.engine.stereo_gl" ||
      id == "view.engine.gdi") {
    Scene3dEngine engine = Scene3dEngine::kFlyCube;
    const char* label = "FlyCube/DX12";
    if (id == "view.engine.stereo_gl") {
      engine = Scene3dEngine::kStereoGl;
      label = "Stereo/GL";
    } else if (id == "view.engine.gdi") {
      engine = Scene3dEngine::kGdi;
      label = "GDI";
    }
    set_scene3d_engine(engine);
    session_.scene3d().set_render_engine_name(label);
    if (engine != Scene3dEngine::kStereoGl) {
      session_.scene3d_stereo().release();
    }
    if (ui::views::MapViewport* scene = ui_->map_scene_viewport()) {
      scene->detach();
      (void)scene->attach();
      if (HWND hwnd = scene->native_view()) {
        if (prefer_scene3d_stereo_gl()) {
          (void)session_.scene3d_stereo().try_attach(hwnd);
        }
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        if (rc.right > 0 && rc.bottom > 0) {
          SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED,
                       MAKELPARAM(rc.right, rc.bottom));
        }
        scene->invalidate_native();
      }
    }
    ui_->set_status_message(std::string("3D engine: ") + label);
    return true;
  }
  if (id == "selection.clear") {
    session_.document().clear_selection();
    host->execute("flash.stop", view_id);
    flash_lit_ = true;
    ui_->sync_flash_timer();
    if (ui_->feature_info()) {
      ui_->feature_info()->clear();
    }
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    ui_->set_status_message("Selection cleared");
    return true;
  }
  if (id == "flash.start" || id == "flash.stop") {
    flash_lit_ = true;
    ui_->sync_flash_timer();
    ui_->invalidate_map_overlays();
    return true;
  }
  if (id == "edit.undo") {
    ui_->set_status_message("Undo");
    return true;
  }
  if (id == "edit.redo") {
    ui_->set_status_message("Redo");
    return true;
  }
  if (id == "edit.cancel") {
    ui_->set_status_message("Edit cancelled");
    return true;
  }
  ui_->set_status_message("Tool: " + id);
  return true;
}

void Browser::on_open() {
  const OpenFileCommand cmd = run_open_file();
  if (!cmd.accepted) {
    return;
  }
  ui::views::MapViewport* pane = ui_->active_map();
  content::MapContents* session =
      pane && pane->map_contents() ? pane->map_contents() : session_.map_contents();
  if (session) {
    detail::catalog_call(session, std::string("{\"op\":\"open\",\"path\":\"") +
                                      detail::json_escape(cmd.path) + "\"}");
  }
  session_.document().open_path(cmd.path);
  // Sibling Style JSON (path.style.json or china_city.style.json beside path).
  {
    std::string style_cand = cmd.path + ".style.json";
    if (!session_.document().load_style_path(style_cand)) {
      const size_t slash = cmd.path.find_last_of("/\\");
      const std::string dir =
          slash == std::string::npos ? std::string()
                                     : cmd.path.substr(0, slash + 1);
      session_.document().load_style_path(dir + "china_city.style.json");
    }
  }
  ui_->sync_catalog_from_scene();
  ui_->sync_inspectors_from_scene();
  fit_map_extent();
  if (ui_->catalog_view()) {
    ui_->catalog_view()->set_map_docs(
        {{cmd.path, "", detail::path_stem(cmd.path), false}});
    ui_->catalog_view()->set_source_names({detail::path_stem(cmd.path)});
  }
  if (content::ViewHost* host = ui_->active_view_host()) {
    const uint32_t view_id = pane ? pane->view_id() : 0;
    host->execute("view.refresh", view_id);
  }
  if (ui_->status_bar()) {
    const char* kind = session_.document().last_open_was_ogr() ? "OGR" : "sample";
    ui_->status_bar()->set_status(std::string("Opened (") + kind + "): " + cmd.path +
                            " (" + std::to_string(session_.document().layer_count()) +
                            " layers, " +
                            std::to_string(session_.document().feature_count()) +
                            " features)");
  }
}

void Browser::on_save_document() {
  const ui::views::FilePickerResult file = ui::views::pick_save_file(
      ui_->hwnd(), L"GeoJSON\0*.geojson\0All\0*.*\0");
  if (!file.accepted || file.path.empty()) {
    ui_->set_status_message("Save cancelled");
    return;
  }
  if (!session_.document().write_path(file.path)) {
    ui_->set_status_message("Save failed");
    return;
  }
  ui_->set_status_message(std::string("Saved (OGR): ") + file.path);
}

void Browser::on_export_document() {
  const ui::views::FilePickerResult file = ui::views::pick_save_file(
      ui_->hwnd(), L"Bitmap\0*.bmp\0All\0*.*\0");
  if (!file.accepted || file.path.empty()) {
    ui_->set_status_message("Export cancelled");
    return;
  }
  int w = 1280;
  int h = 720;
  ui_->active_view_size(&w, &h);
  if (w <= 0) {
    w = 1280;
  }
  if (h <= 0) {
    h = 720;
  }
  if (!session_.map2d().export_bmp(file.path, w, h)) {
    ui_->set_status_message("Export BMP failed");
    return;
  }
  ui_->set_status_message(std::string("Exported BMP: ") + file.path);
}

}  // namespace app
