// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser_view.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/app_commands.h"
#include "app/views/plugin_chrome.h"
#include "content/public/events.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "sdb/edit/edit_session.h"
#include "sdb/tile/tile_map_layer.h"
#include "tool/command.h"
#include "tool/workspace.h"
#include "ui/views/add_basemap_dialog.h"
#include "ui/views/ambox_view.h"
#include "ui/views/att_struct_dialog.h"
#include "ui/views/attribute_table.h"
#include "ui/views/catalog_view.h"
#include "ui/views/create_datasource_dialog.h"
#include "ui/views/create_layer_dialog.h"
#include "ui/views/create_map_dialog.h"
#include "ui/views/feature_info.h"
#include "ui/views/file_picker.h"
#include "ui/views/input_text_dialog.h"
#include "ui/views/layer_tree.h"
#include "ui/views/layout.h"
#include "ui/views/map_viewport.h"
#include "ui/views/menu_bar.h"
#include "ui/views/splitter.h"
#include "ui/views/status_bar.h"
#include "ui/views/tab_strip.h"
#include "ui/views/view.h"

namespace app {
namespace detail {

std::string wide_to_utf8(const wchar_t* text) {
  if (!text || !text[0]) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr,
                                    nullptr);
  std::string out(n > 0 ? static_cast<size_t>(n - 1) : 0, '\0');
  if (n > 1) {
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), n, nullptr, nullptr);
  }
  return out;
}

std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

void catalog_call(content::MapContents* session, const std::string& json) {
  if (!session) {
    return;
  }
  session->CatalogCall(json.c_str());
}

// Chrome-side layer mirror after CatalogCall open. Content has no CatalogDelta
// payload yet; until then the host seeds LayerTree from the opened path stem.
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

std::vector<ui::views::LayerTree::LayerDesc> layers_from_open_path(
    const std::string& path) {
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  const std::string stem = path_stem(path);
  if (stem.empty()) {
    return layers;
  }
  ui::views::LayerTree::LayerDesc layer;
  layer.id = path;
  layer.name = stem;
  layer.visible = true;
  layer.active = true;
  layers.push_back(std::move(layer));
  return layers;
}

void populate_catalog_after_open(ui::views::CatalogView* catalog,
                                 const std::string& path) {
  if (!catalog) {
    return;
  }
  const auto layers = layers_from_open_path(path);
  if (!layers.empty()) {
    catalog->populate_layers(layers);
    catalog->set_map_docs({{path, "", path_stem(path), false}});
  }
}

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

// Map AttributeTable opaque tokens to FeatureId for EditSession::commit.
// Accepts "fid:" + hex (selection feedback) or raw opaque bytes.
content::FeatureId feature_id_from_opaque_token(const std::string& token) {
  content::FeatureId id{};
  if (token.empty()) {
    return id;
  }
  std::string_view hex = token;
  constexpr std::string_view kPrefix = "fid:";
  if (hex.size() >= kPrefix.size() && hex.substr(0, kPrefix.size()) == kPrefix) {
    hex.remove_prefix(kPrefix.size());
  }
  const bool even_hex =
      !hex.empty() && (hex.size() % 2) == 0 && hex.size() <= 64;
  if (even_hex) {
    bool ok = true;
    uint8_t bytes[32] = {};
    size_t n = 0;
    for (size_t i = 0; i + 1 < hex.size() && n < sizeof(bytes); i += 2) {
      const int hi = hex_nibble(hex[i]);
      const int lo = hex_nibble(hex[i + 1]);
      if (hi < 0 || lo < 0) {
        ok = false;
        break;
      }
      bytes[n++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    if (ok && n > 0) {
      id.len = static_cast<uint8_t>(n);
      std::memcpy(id.bytes, bytes, n);
      return id;
    }
  }
  id.len = static_cast<uint8_t>(
      token.size() < sizeof(id.bytes) ? token.size() : sizeof(id.bytes));
  std::memcpy(id.bytes, token.data(), id.len);
  return id;
}

}  // namespace detail

BrowserView::BrowserView() = default;

BrowserView::~BrowserView() {
  if (plugins_) {
    plugins_->shutdown();
    plugins_.reset();
  }
}

bool BrowserView::init() {
  ui::views::Widget::InitParams params;
  params.title = L"SmartGIS Views";
  params.width = 1280;
  params.height = 800;
  if (!widget_.init(params)) {
    return false;
  }

  edit_host_ = std::make_unique<content::ViewHost>();
  data_host_ = std::make_unique<content::ViewHost>();
  scene_host_ = std::make_unique<content::ViewHost>();
  map_session_.reset(content::MapContents::Create());
  if (map_session_ && !map_session_->StartRenderProcess()) {
    map_session_.reset();
  }

  plugins_ = std::make_unique<PluginChrome>();
  if (!plugins_->init(edit_host_->events())) {
    plugins_.reset();
    return false;
  }

  build_contents();
  attach_viewports();
  wire_edit_feedback();
  sync_status();
  return true;
}

void BrowserView::show() {
  widget_.show();
}

int BrowserView::run_loop() {
  return widget_.run_loop();
}

HWND BrowserView::hwnd() const {
  return widget_.hwnd();
}

ui::views::View* BrowserView::contents_view() const {
  return widget_.contents_view();
}

void BrowserView::build_contents() {
  auto root = std::make_unique<ui::views::View>();
  auto root_box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);

  auto menu = std::make_unique<ui::views::MenuBar>();
  menu->set_preferred_size({0, 28});
  menu->add_item("Open", [this]() { on_open(); });
  menu->add_item("Exit", [this]() { on_exit(); });
  menu->add_item("Map Edit", [this]() { switch_map_tab(0); });
  menu->add_item("Datasource", [this]() { switch_map_tab(1); });
  menu->add_item("3D", [this]() { switch_map_tab(2); });
  menu->add_item("Select", [this]() { run_tool_command("selection.point"); });
  menu->add_item("Draw Point",
                 [this]() { run_tool_command("edit.append.point"); });
  menu->add_item("Draw Line",
                 [this]() { run_tool_command("edit.append.linestring"); });
  menu->add_item("Draw Polygon",
                 [this]() { run_tool_command("edit.append.polygon"); });
  menu->add_item("Clear Sel",
                 [this]() { run_tool_command("selection.clear"); });
  menu->add_item("Undo", [this]() { run_tool_command("edit.undo"); });
  menu->add_item("Plugins", [this]() { on_plugins(); });

  auto catalog = std::make_unique<ui::views::CatalogView>();
  catalog->set_preferred_size({240, 0});
  catalog->set_title("Catalog");
  catalog_ = catalog.get();
  wire_catalog();

  auto map_tabs = std::make_unique<ui::views::TabStrip>();
  auto map_edit = std::make_unique<ui::views::MapViewport>();
  auto map_data = std::make_unique<ui::views::MapViewport>();
  auto map_scene = std::make_unique<ui::views::MapViewport>();
  map_edit_ = map_edit.get();
  map_data_ = map_data.get();
  map_scene_ = map_scene.get();
  map_edit_->set_role(ui::views::MapViewport::Role::kMapEdit);
  map_data_->set_role(ui::views::MapViewport::Role::kMapData);
  map_scene_->set_role(ui::views::MapViewport::Role::kScene3d);
  map_tabs->add_tab("Map", std::move(map_edit));
  map_tabs->add_tab("Data", std::move(map_data));
  map_tabs->add_tab("3D", std::move(map_scene));
  map_tabs->set_change([this](int i) { switch_map_tab(i); });
  map_tabs_ = map_tabs.get();

  auto catalog_map = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kHorizontal);
  catalog_map->set_preferred_size({0, 0});
  catalog_map->add_child(std::move(catalog));
  catalog_map->add_child(std::move(map_tabs));

  auto ambox = std::make_unique<ui::views::AmboxView>();
  ambox->set_preferred_size({200, 0});
  ambox_ = ambox.get();
  ambox_->set_command_handler([this](const std::string& id) {
    if (plugins_ && plugins_->execute(id)) {
      return;
    }
    run_tool_command(id);
  });
  populate_ambox();

  auto work = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kHorizontal);
  work->set_preferred_size({0, 0});
  work->add_child(std::move(catalog_map));
  work->add_child(std::move(ambox));

  auto feature_info = std::make_unique<ui::views::FeatureInfo>();
  feature_info_ = feature_info.get();
  auto attribute_table = std::make_unique<ui::views::AttributeTable>();
  attribute_table_ = attribute_table.get();
  attribute_table_->set_on_cell_commit(
      [this](const std::string& feature_token, const std::string& field,
             const std::string& value) {
        content::ViewHost* host = active_view_host();
        if (!host || !host->edits()) {
          set_status_message("Attribute edit failed: no edit session");
          return false;
        }
        sdb::FeatureMutation mutation;
        mutation.op = sdb::EditOp::kModify;
        mutation.id = detail::feature_id_from_opaque_token(feature_token);
        if (mutation.id.len == 0) {
          set_status_message("Attribute edit failed: empty feature token");
          return false;
        }
        // Field/value stay at the string boundary; EditSession mutation is
        // currently id-scoped (attribute payload TBD on FeatureMutation).
        if (!host->edits()->commit(mutation)) {
          set_status_message("Attribute edit failed: " + field);
          return false;
        }
        set_status_message("Updated " + field + "=" + value);
        return true;
      });

  auto inspector = std::make_unique<ui::views::TabStrip>();
  inspector->set_preferred_size({0, 160});
  inspector->add_tab("FeatureInfo", std::move(feature_info));
  inspector->add_tab("AttributeTable", std::move(attribute_table));

  auto columns = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kVertical);
  columns->add_child(std::move(work));
  columns->add_child(std::move(inspector));

  auto status = std::make_unique<ui::views::StatusBar>();
  status->set_preferred_size({0, 24});
  status_bar_ = status.get();

  root_box->set_flex_for_view(columns.get(), 1);
  root->set_layout_manager(std::move(root_box));
  root->add_child(std::move(menu));
  root->add_child(std::move(columns));
  root->add_child(std::move(status));

  widget_.set_contents_view(std::move(root));
}

void BrowserView::attach_viewports() {
  struct Bind {
    ui::views::MapViewport* pane;
    content::ViewHost* host;
    const char* tool;
  };
  const Bind binds[] = {
      {map_edit_, edit_host_.get(), "view.pan"},
      {map_data_, data_host_.get(), "view.pan"},
      {map_scene_, scene_host_.get(), "view3d.trackball"},
  };
  for (const Bind& b : binds) {
    if (!b.pane) {
      continue;
    }
    b.pane->set_view_host(b.host);
    if (map_session_) {
      b.pane->set_map_contents(map_session_.get());
    }
    b.pane->attach();
    if (b.host) {
      b.host->activate(b.tool);
    }
  }
}

void BrowserView::wire_catalog() {
  if (!catalog_ || !catalog_->layer_tree()) {
    return;
  }
  catalog_->populate_demo_layers();
  catalog_->set_source_names({"Memory"});
  catalog_->set_map_docs(
      {{"map.untitled", "", "Untitled map", false}});
  catalog_->set_command(
      [this](const std::string& id) { on_catalog_command(id); });
  catalog_->layer_tree()->set_visible_changed(
      [this](const std::string& id, bool visible) {
        content::MapContents* session =
            active_map() ? active_map()->map_contents() : map_session_.get();
        detail::catalog_call(
            session, std::string("{\"op\":\"set_visible\",\"id\":\"") +
                         detail::json_escape(id) + "\",\"visible\":" +
                         (visible ? "true" : "false") + "}");
        if (content::ViewHost* host = active_view_host()) {
          const uint32_t view_id =
              active_map() ? active_map()->view_id() : 0;
          host->execute("view.refresh", view_id);
        }
        if (status_bar_) {
          status_bar_->set_message(std::string("Layer ") + id +
                                   (visible ? ": visible" : ": hidden"));
        }
      });
  catalog_->layer_tree()->set_selection_changed([this](const std::string& id) {
    content::MapContents* session =
        active_map() ? active_map()->map_contents() : map_session_.get();
    detail::catalog_call(session,
                         std::string("{\"op\":\"select_layer\",\"id\":\"") +
                             detail::json_escape(id) + "\"}");
    if (content::ViewHost* host = active_view_host()) {
      const uint32_t view_id = active_map() ? active_map()->view_id() : 0;
      host->execute("view.refresh", view_id);
    }
    if (status_bar_) {
      status_bar_->set_message("Active layer: " + id);
    }
  });
}

void BrowserView::wire_edit_feedback() {
  if (!edit_host_ || !edit_host_->events()) {
    return;
  }
  selection_sub_ = edit_host_->events()->subscribe<content::SelectionChanged>(
      [this](const content::SelectionChanged& ev) {
        if (ev.ids.empty()) {
          set_status_message("Selection cleared");
          if (feature_info_) {
            feature_info_->clear();
          }
          return;
        }
        set_status_message("Selected " + std::to_string(ev.ids.size()) +
                           " feature(s)");
        if (feature_info_) {
          // Opaque token only — never SmtFeature*.
          const content::FeatureId& fid = ev.ids.front();
          std::string token = "fid:";
          for (uint8_t i = 0; i < fid.len && i < sizeof(fid.bytes); ++i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02x",
                          static_cast<unsigned>(fid.bytes[i]));
            token += hex;
          }
          feature_info_->set_feature_id(std::move(token));
        }
      });
  edit_sub_ = edit_host_->events()->subscribe<content::EditCommitted>(
      [this](const content::EditCommitted& ev) {
        const char* op = "modify";
        if (ev.op == content::EditCommitted::Op::kAppend) {
          op = "append";
        } else if (ev.op == content::EditCommitted::Op::kDelete) {
          op = "delete";
        }
        set_status_message(std::string("Committed ") + op);
      });
}

void BrowserView::populate_ambox() {
  if (!ambox_) {
    return;
  }
  std::vector<tool::CommandCatalog*> catalogs;
  if (edit_host_ && edit_host_->workspace()) {
    catalogs.push_back(&edit_host_->workspace()->catalog());
  }
  if (plugins_) {
    if (tool::CommandCatalog* plugin_catalog = plugins_->commands()) {
      catalogs.push_back(plugin_catalog);
    }
  }
  ambox_->populate_from_commands(catalogs);
}

bool BrowserView::run_tool_command(std::string_view command_id) {
  content::ViewHost* host = active_view_host();
  if (!host || command_id.empty()) {
    return false;
  }
  std::string id(command_id);
  if (id == "select" || id == "identify") {
    id = "selection.point";
  } else if (id == "pan") {
    id = "view.pan";
  }
  const uint32_t view_id = active_map() ? active_map()->view_id() : 0;
  if (!host->execute(id, view_id)) {
    set_status_message("Unknown tool " + id);
    return false;
  }
  if (id == "selection.clear") {
    // SelectionChanged subscriber also updates status.
    return true;
  }
  if (id == "edit.undo") {
    set_status_message("Undo");
    return true;
  }
  if (id == "edit.redo") {
    set_status_message("Redo");
    return true;
  }
  if (id == "edit.cancel") {
    set_status_message("Edit cancelled");
    return true;
  }
  set_status_message("Tool: " + id);
  return true;
}

void BrowserView::set_status_message(const std::string& text) {
  if (status_bar_) {
    status_bar_->set_message(text);
  }
}

void BrowserView::on_catalog_command(const std::string& command_id) {
  const HWND hwnd = widget_.hwnd();
  content::MapContents* session =
      active_map() ? active_map()->map_contents() : map_session_.get();
  auto refresh = [this]() {
    if (content::ViewHost* host = active_view_host()) {
      const uint32_t view_id = active_map() ? active_map()->view_id() : 0;
      host->execute("view.refresh", view_id);
    }
  };
  auto status = [this](const std::string& text) {
    if (status_bar_) {
      status_bar_->set_message(text);
    }
  };

  if (command_id == "catalog.layer.create") {
    ui::views::CreateLayerDialog::Result out;
    if (ui::views::CreateLayerDialog::run(hwnd, &out) && catalog_ &&
        catalog_->layer_tree()) {
      if (catalog_->using_demo_layers()) {
        catalog_->populate_layers({{out.name, out.name, true, true}});
      } else {
        catalog_->layer_tree()->add_layer(out.name, out.name, true);
        catalog_->layer_tree()->select_layer(out.name);
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
    sdb::MapLayer layer;
    if (out.kind == "wmts") {
      layer = sdb::tile::make_wmts_map_layer(out.url);
    } else {
      layer = sdb::tile::make_xyz_map_layer(out.url);
    }
    if (layer.leftover() == nullptr) {
      status("Basemap URL rejected");
      return;
    }
    const std::string name =
        out.name.empty() ? std::string("Basemap") : out.name;
    if (catalog_ && catalog_->layer_tree()) {
      if (catalog_->using_demo_layers()) {
        catalog_->populate_layers({{name, name, true, true}});
      } else {
        catalog_->layer_tree()->add_layer(name, name, true);
        catalog_->layer_tree()->select_layer(name);
      }
    }
    // Content/render still consumes CatalogCall JSON; tile MapLayer is
    // validated here. Full scene attach lands when map host accepts kind=tile.
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
    if (ui::views::CreateMapDialog::run(hwnd, &name) && catalog_) {
      catalog_->set_map_docs({{name, "", name, false}});
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
    if (ui::views::CreateDatasourceDialog::run(hwnd, &out) && catalog_) {
      catalog_->set_source_names({out.name});
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
    std::vector<ui::views::AttField> fields;
    if (ui::views::AttStructDialog::run(hwnd, &fields)) {
      status("Attribute structure (" +
             std::to_string(fields.size()) + " fields)");
    }
    return;
  }
  if (command_id == "catalog.layer.append" ||
      command_id == "catalog.layer.property" ||
      command_id == "catalog.ds.property") {
    std::string text;
    if (ui::views::InputTextDialog::run(hwnd, L"Input", "Name", &text) &&
        catalog_ && catalog_->layer_tree() && !text.empty()) {
      if (catalog_->using_demo_layers()) {
        catalog_->populate_layers({{text, text, true, true}});
      } else {
        catalog_->layer_tree()->add_layer(text, text, true);
        catalog_->layer_tree()->select_layer(text);
      }
      status(command_id + ": " + text);
    }
    return;
  }
  if (command_id == "catalog.layer.load_shp" ||
      command_id == "catalog.layer.load_image" ||
      command_id == "catalog.map.open") {
    const ui::views::FilePickerResult file = ui::views::pick_open_file(
        command_id == "catalog.layer.load_image"
            ? L"Images\0*.tif;*.img;*.png;*.jpg\0All\0*.*\0"
            : L"GIS\0*.shp;*.gpkg;*.smtmap\0All\0*.*\0");
    if (file.accepted) {
      detail::catalog_call(
          session, std::string("{\"op\":\"open\",\"path\":\"") +
                       detail::json_escape(file.path) + "\"}");
      detail::populate_catalog_after_open(catalog_, file.path);
      refresh();
      status("Opened " + file.path);
    }
    return;
  }
  if (command_id == "catalog.map.save" || command_id == "catalog.map.save_as") {
    const ui::views::FilePickerResult file =
        ui::views::pick_save_file(L"Map\0*.smtmap\0All\0*.*\0");
    if (file.accepted) {
      detail::catalog_call(
          session, std::string("{\"op\":\"save\",\"path\":\"") +
                       detail::json_escape(file.path) + "\"}");
      status("Saved " + file.path);
    }
    return;
  }
  if (command_id == "catalog.layer.remove" ||
      command_id == "catalog.layer.delete") {
    const std::string id =
        catalog_ && catalog_->layer_tree()
            ? catalog_->layer_tree()->selected_id()
            : std::string();
    if (!id.empty()) {
      detail::catalog_call(
          session, std::string("{\"op\":\"remove_layer\",\"id\":\"") +
                       detail::json_escape(id) + "\"}");
      refresh();
      status("Removed " + id);
    }
    return;
  }
  detail::catalog_call(session, std::string("{\"op\":\"command\",\"id\":\"") +
                                     detail::json_escape(command_id) + "\"}");
  status(command_id);
}

void BrowserView::on_open() {
  const OpenFileCommand cmd = run_open_file();
  if (!cmd.accepted) {
    return;
  }
  ui::views::MapViewport* pane = active_map();
  content::MapContents* session =
      pane && pane->map_contents() ? pane->map_contents() : map_session_.get();
  if (session) {
    detail::catalog_call(session, std::string("{\"op\":\"open\",\"path\":\"") +
                                      detail::json_escape(cmd.path) + "\"}");
  }
  detail::populate_catalog_after_open(catalog_, cmd.path);
  if (content::ViewHost* host = active_view_host()) {
    const uint32_t view_id = pane ? pane->view_id() : 0;
    host->execute("view.refresh", view_id);
  }
  if (status_bar_) {
    if (session) {
      const size_t n =
          catalog_ && catalog_->layer_tree()
              ? catalog_->layer_tree()->layer_count()
              : 0;
      status_bar_->set_status("Opened: " + cmd.path + " (" +
                              std::to_string(n) + " layers)");
    } else {
      status_bar_->set_status(cmd.path);
    }
  }
}

void BrowserView::on_exit() {
  if (HWND hwnd = widget_.hwnd()) {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
  } else {
    PostQuitMessage(0);
  }
}

void BrowserView::on_plugins() {
  if (plugins_) {
    plugins_->show_manager(widget_.hwnd());
    // Plugin enable/disable may change contributed command ids.
    populate_ambox();
  }
}

void BrowserView::switch_map_tab(int i) {
  if (map_tabs_ && map_tabs_->active() != i) {
    map_tabs_->set_active(i);
  }
  // TabStrip show/hides native map HWNDs; never destroy/recreate on switch.
  if (ui::views::MapViewport* pane = active_map()) {
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        // Force a size notify so content/FlyCube surfaces follow the tab body.
        if (rc.right > 0 && rc.bottom > 0) {
          SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED,
                       MAKELPARAM(rc.right, rc.bottom));
        }
      }
    }
  }
  if (content::ViewHost* host = active_view_host()) {
    if (i == 2) {
      // Basic pan/orbit for the 3D tab when a ViewHost is wired.
      host->activate("view3d.trackball");
    } else {
      host->activate("view.pan");
    }
  }
  sync_status();
}

void BrowserView::sync_status() {
  ui::views::MapViewport* pane = active_map();
  if (!status_bar_ || !pane) {
    return;
  }
  status_bar_->set_status(detail::wide_to_utf8(pane->status_text()));
}

ui::views::MapViewport* BrowserView::active_map() const {
  const int i = map_tabs_ ? map_tabs_->active() : 0;
  if (i == 1) {
    return map_data_;
  }
  if (i == 2) {
    return map_scene_;
  }
  return map_edit_;
}

content::ViewHost* BrowserView::active_view_host() const {
  const int i = map_tabs_ ? map_tabs_->active() : 0;
  if (i == 1) {
    return data_host_.get();
  }
  if (i == 2) {
    return scene_host_.get();
  }
  return edit_host_.get();
}

}  // namespace app
