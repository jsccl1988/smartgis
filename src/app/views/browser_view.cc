// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser_view.h"

#include <memory>
#include <string>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/app_commands.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"
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

}  // namespace detail

BrowserView::BrowserView() = default;

BrowserView::~BrowserView() = default;

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

  build_contents();
  attach_viewports();
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
  ambox->set_command_handler([this](const std::string& id) {
    content::ViewHost* host = active_view_host();
    if (!host) {
      return;
    }
    if (id == "select" || id == "identify") {
      host->activate("selection.point");
      return;
    }
    if (id == "pan") {
      host->activate("view.pan");
      return;
    }
    host->execute(id);
  });

  auto work = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kHorizontal);
  work->set_preferred_size({0, 0});
  work->add_child(std::move(catalog_map));
  work->add_child(std::move(ambox));

  auto inspector = std::make_unique<ui::views::TabStrip>();
  inspector->set_preferred_size({0, 160});
  inspector->add_tab("FeatureInfo", std::make_unique<ui::views::FeatureInfo>());
  inspector->add_tab("AttributeTable",
                     std::make_unique<ui::views::AttributeTable>());

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
  catalog_->layer_tree()->add_layer("layer.demo", "Demo layer", true);
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
  });
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
      catalog_->layer_tree()->add_layer(out.name, out.name, true);
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
      catalog_->layer_tree()->add_layer(text, text, true);
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
  if (content::ViewHost* host = active_view_host()) {
    const uint32_t view_id = pane ? pane->view_id() : 0;
    host->execute("view.refresh", view_id);
  }
  if (status_bar_) {
    if (session) {
      status_bar_->set_status("Opened: " + cmd.path);
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

void BrowserView::switch_map_tab(int i) {
  if (map_tabs_ && map_tabs_->active() != i) {
    map_tabs_->set_active(i);
  }
  if (content::ViewHost* host = active_view_host()) {
    if (i == 2) {
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
