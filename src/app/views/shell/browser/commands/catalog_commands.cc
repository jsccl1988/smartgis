// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

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
#include "plugin/runtime/host/registry/registry.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "vista/atmosphere/session/field_channel.h"
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

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace app {

namespace detail {

// Escape for embedding inside a JSON double-quoted value (RapidJSON Writer).
std::string json_escape(const std::string& text) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.String(text.c_str(), static_cast<rapidjson::SizeType>(text.size()));
  const char* s = buf.GetString();
  const size_t n = buf.GetSize();
  if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
    return std::string(s + 1, n - 2);
  }
  return std::string(s, n);
}

void catalog_call(content::MapContents* session, const std::string& json) {
  if (!session) {
    return;
  }
  session->CatalogCall(json.c_str());
}

std::string path_stem(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  size_t begin = path.find_last_of("/\\");
  begin = (begin == std::string::npos) ? 0 : begin + 1;
  size_t end = path.find_last_of('.');
  if (end == std::string::npos || end < begin) {
    end = path.size();
  }
  return path.substr(begin, end - begin);
}

}  // namespace detail

// Catalog context-menu commands and layer mutations.

void Browser::on_catalog_command(const std::string& command_id) {
  const HWND hwnd = ui_->hwnd();
  content::MapContents* session =
      ui_->active_map() ? ui_->active_map()->map_contents() : session_.map_contents();
  auto refresh = [this]() {
    if (content::ViewHost* host = ui_->active_view_host()) {
      const uint32_t view_id = ui_->active_map() ? ui_->active_map()->view_id() : 0;
      host->execute("view.refresh", view_id);
    }
  };
  auto status = [this](const std::string& text) {
    if (ui_->status_bar()) {
      ui_->status_bar()->set_message(text);
    }
  };

  if (command_id == "catalog.layer.create") {
    ui::views::CreateLayerDialog::Result out;
    if (ui::views::CreateLayerDialog::run(hwnd, &out)) {
      if (session_.document().create_layer(out.name, out.geometry_type)) {
        ui_->sync_catalog_from_scene();
        ui_->sync_inspectors_from_scene();
        ui_->invalidate_map_overlays();
      }
      detail::catalog_call(
          session, std::string("{\"op\":\"create_layer\",\"name\":\"") +
                       detail::json_escape(out.name) +
                       "\",\"geometry\":\"" +
                       detail::json_escape(out.geometry_type) + "\"}");
      refresh();
      status("Created layer " + out.name);
    }
    return;
  }
  if (command_id == "catalog.layer.add_basemap") {
    ui::views::AddBasemapDialog::Result out;
    if (!ui::views::AddBasemapDialog::run(hwnd, &out) || out.url.empty()) {
      return;
    }
    auto provider = std::make_shared<gis::tile::TileProvider>();
    const bool opened = (out.kind == "wmts")
                            ? provider->open_wmts_template(out.url)
                            : provider->open_xyz(out.url);
    if (!opened) {
      status("Basemap URL rejected");
      return;
    }
    // Keep MapLayer validation so CatalogCall JSON stays meaningful.
    gis::MapLayer layer = (out.kind == "wmts")
                              ? gis::tile::make_wmts_map_layer(out.url)
                              : gis::tile::make_xyz_map_layer(out.url);
    if (layer.tile() == nullptr) {
      status("Basemap URL rejected");
      return;
    }
    const std::string name =
        out.name.empty() ? std::string("Basemap") : out.name;
    session_.document().create_layer(name, out.kind);
    session_.document().set_basemap_provider(std::move(provider));
    ui_->sync_catalog_from_scene();
    ui_->sync_inspectors_from_scene();
    ui_->invalidate_map_overlays();
    detail::catalog_call(
        session, std::string("{\"op\":\"add_basemap\",\"name\":\"") +
                     detail::json_escape(name) + "\",\"kind\":\"" +
                     detail::json_escape(out.kind) + "\",\"url\":\"" +
                     detail::json_escape(out.url) + "\"}");
    refresh();
    status("Basemap " + name);
    return;
  }
  if (command_id == "catalog.map.create") {
    std::string name;
    if (ui::views::CreateMapDialog::run(hwnd, &name) && ui_->catalog_view()) {
      ui_->catalog_view()->set_map_docs({{name, "", name, false}});
      detail::catalog_call(
          session, std::string("{\"op\":\"create_map\",\"name\":\"") +
                       detail::json_escape(name) + "\"}");
      refresh();
      status("Created map " + name);
    }
    return;
  }
  if (command_id == "catalog.ds.create" || command_id == "catalog.ds.append") {
    ui::views::CreateDatasourceDialog::Result out;
    if (ui::views::CreateDatasourceDialog::run(hwnd, &out) && ui_->catalog_view()) {
      ui_->catalog_view()->set_source_names({out.name});
      detail::catalog_call(
          session, std::string("{\"op\":\"create_datasource\",\"name\":\"") +
                       detail::json_escape(out.name) + "\",\"type\":\"" +
                       detail::json_escape(out.type) + "\"}");
      refresh();
      status("Datasource " + out.name);
    }
    return;
  }
  if (command_id == "catalog.layer.attstruct") {
    std::vector<ui::views::AttributeField> fields;
    if (ui::views::AttributeSchemaDialog::run(hwnd, &fields)) {
      status("Attribute schema (" +
             std::to_string(fields.size()) + " fields)");
    }
    return;
  }
  if (command_id == "catalog.layer.append" ||
      command_id == "catalog.layer.property" ||
      command_id == "catalog.ds.property") {
    std::string text;
    if (ui::views::InputTextDialog::run(hwnd, L"Input", "Name", &text) &&
        !text.empty()) {
      if (command_id == "catalog.layer.append") {
        session_.document().create_layer(text, "point");
        ui_->sync_catalog_from_scene();
        ui_->sync_inspectors_from_scene();
        ui_->invalidate_map_overlays();
      }
      status(command_id + ": " + text);
    }
    return;
  }
  if (command_id == "catalog.layer.load_shp" ||
      command_id == "catalog.layer.load_image" ||
      command_id == "catalog.map.open") {
    const ui::views::FilePickerResult file = ui::views::pick_open_file(
        hwnd,
        command_id == "catalog.layer.load_image"
            ? L"Images\0*.tif;*.img;*.png;*.jpg\0All\0*.*\0"
            : L"GIS\0*.shp;*.gpkg;*.geojson;*.json;*.smtmap\0All\0*.*\0");
    if (file.accepted) {
      detail::catalog_call(
          session, std::string("{\"op\":\"open\",\"path\":\"") +
                       detail::json_escape(file.path) + "\"}");
      const bool ogr_ok = session_.document().open_path(file.path);
      ui_->sync_catalog_from_scene();
      ui_->sync_inspectors_from_scene();
      fit_map_extent();
      refresh();
      if (ogr_ok) {
        status("OGR opened " + file.path + " (" +
               std::to_string(session_.document().layer_count()) + " layers, " +
               std::to_string(session_.document().feature_count()) + " features)");
      } else {
        status("Opened (sample fallback) " + file.path);
      }
    }
    return;
  }
  if (command_id == "catalog.map.save" || command_id == "catalog.map.save_as") {
    on_save_document();
    return;
  }
  if (command_id == "catalog.layer.remove" ||
      command_id == "catalog.layer.delete") {
    const std::string id =
        ui_->catalog_view() && ui_->catalog_view()->layer_tree()
            ? ui_->catalog_view()->layer_tree()->selected_id()
            : std::string();
    if (!id.empty() && session_.document().remove_layer(id)) {
      ui_->sync_catalog_from_scene();
      ui_->sync_inspectors_from_scene();
      ui_->invalidate_map_overlays();
      detail::catalog_call(
          session, std::string("{\"op\":\"remove_layer\",\"id\":\"") +
                       detail::json_escape(id) + "\"}");
      refresh();
      status("Removed " + id);
    }
    return;
  }
  if (command_id == "catalog.layer.move_up" ||
      command_id == "catalog.layer.move_down") {
    const std::string id =
        ui_->catalog_view() && ui_->catalog_view()->layer_tree()
            ? ui_->catalog_view()->layer_tree()->selected_id()
            : std::string();
    const int delta = (command_id == "catalog.layer.move_up") ? -1 : 1;
    if (!id.empty() && session_.document().move_layer(id, delta)) {
      ui_->sync_catalog_from_scene();
      ui_->invalidate_map_overlays();
      status(delta < 0 ? "Layer moved up" : "Layer moved down");
    }
    return;
  }
  if (command_id == "catalog.layer.active") {
    const std::string id =
        ui_->catalog_view() && ui_->catalog_view()->layer_tree()
            ? ui_->catalog_view()->layer_tree()->selected_id()
            : std::string();
    if (!id.empty() && session_.document().select_layer(id)) {
      ui_->sync_catalog_from_scene();
      status("Active layer: " + id);
    }
    return;
  }
  if (command_id == "catalog.layer.view" ||
      command_id == "catalog.layer.recalc_mbr") {
    fit_map_extent();
    detail::catalog_call(
        session, std::string("{\"op\":\"view_extent\",\"id\":\"") +
                     detail::json_escape(
                         ui_->catalog_view() && ui_->catalog_view()->layer_tree()
                             ? ui_->catalog_view()->layer_tree()->selected_id()
                             : std::string()) +
                     "\"}");
    status(command_id == "catalog.layer.view" ? "View layer extent"
                                              : "Recalc MBR / refit");
    return;
  }
  detail::catalog_call(session, std::string("{\"op\":\"command\",\"id\":\"") +
                                     detail::json_escape(command_id) + "\"}");
  status(command_id);
}

}  // namespace app
