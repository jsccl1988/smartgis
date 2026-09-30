// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/browser_view.h"

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

#include "content/browser/input/map_hwnd_gestures.h"
#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/commands/app_commands.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "content/public/map_contents.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "plugin/runtime/host/registry.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/nav/camera_nav.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/gis/shell/ambox_view.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/dialogs/select_one_dialog.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"

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

std::string json_escape(const std::string& text);

void catalog_call(content::MapContents* session, const std::string& json);

}  // namespace detail

namespace {

// Copy bookmark labels with a hard cap. A corrupted MapSession layout can
// make bookmarks().size() look huge; vector::reserve then throws length_error
// and CRT abort() — seen at BrowserView::rebuild_menus during init_chrome.
void collect_bookmark_labels(Browser* browser,
                             std::vector<std::string>* labels) {
  if (!browser || !labels) {
    return;
  }
  labels->clear();
  const content::ViewNavigation* nav = browser->navigation();
  if (!nav) {
    return;
  }
  const auto& bookmarks = nav->bookmarks();
  const size_t n = bookmarks.size();
  constexpr size_t kCap = 512;
  if (n == 0 || n > kCap) {
    return;
  }
  labels->reserve(n);
  for (const content::ViewBookmark& mark : bookmarks) {
    labels->push_back(mark.label);
  }
}

std::vector<ui::views::AmboxView::Group> enabled_plugin_groups(
    plugin::Registry* registry,
    content::PluginHost* host) {
  std::vector<ui::views::AmboxView::Group> groups;
  if (!registry) {
    return groups;
  }
  std::map<std::string, size_t> index;
  for (const plugin::PluginRecord& rec : registry->list()) {
    if (rec.state != plugin::PluginState::kEnabled) {
      continue;
    }
    ui::views::AmboxView::Group group;
    group.name =
        rec.manifest.name.empty() ? rec.manifest.id : rec.manifest.name;
    index.emplace(rec.manifest.id, groups.size());
    groups.push_back(std::move(group));
  }
  if (!host) {
    return groups;
  }
  host->for_each_command([&](std::string_view plugin_id,
                             std::string_view command_id,
                             std::string_view title) {
    // Guard against cross-DLL PluginHost vtable slips that pass garbage
    // string_views (would throw bad_alloc / length_error on construct).
    constexpr size_t kMaxId = 256;
    if (plugin_id.empty() || plugin_id.size() > kMaxId ||
        command_id.empty() || command_id.size() > kMaxId ||
        title.size() > kMaxId) {
      return;
    }
    const auto it = index.find(std::string(plugin_id));
    if (it == index.end()) {
      return;
    }
    ui::views::AmboxView::Item item;
    item.id = std::string(command_id);
    item.label = title.empty() ? item.id : std::string(title);
    groups[it->second].items.push_back(std::move(item));
  });
  return groups;
}

}  // namespace

std::unique_ptr<BrowserUiDelegate> create_browser_ui(Browser* browser) {
  return std::make_unique<BrowserView>(browser);
}

BrowserView::BrowserView(Browser* browser) : browser_(browser) {}

BrowserView::~BrowserView() {
  prepare_chrome_close();
}

void BrowserView::prepare_chrome_close() {
  if (map_edit_) {
    map_edit_->detach();
  }
  if (map_data_) {
    map_data_->detach();
  }
  if (map_scene_) {
    map_scene_->detach();
  }
}

bool BrowserView::init_chrome() {
  ui::views::Widget::InitParams params;
  params.title = L"SmartGIS Views";
  // Client DIPs (Widget scales + AdjustWindowRect). Physical-only 1280x800
  // looked ~853x533 on 150% DPI hosts.
  params.width = 1280;
  params.height = 800;
  params.size_in_dips = true;
  params.frame_kind = ui::views::Widget::FrameKind::kCustom;
  if (!widget_.init(params)) {
    return false;
  }
  widget_.set_will_close([this]() {
    if (browser_) {
      browser_->prepare_close();
    }
  });
  widget_.set_on_shell_published(
      [this](const ui::views::Rect& dirty) { commit_widget_shell_to_maps(dirty); });

  build_contents();
  browser_->document()->seed_default();
  browser_->map2d()->bind(browser_->document(), browser_->view_frame());
  browser_->scene3d()->bind_orbit(browser_->orbit_frame());
  browser_->scene3d()->bind_label_frame(browser_->view_frame());
  browser_->scene3d()->bind_map(browser_->document());
  browser_->pull_orbit_extent();
  // Fit world extent BEFORE FlyCube attach so the first display-thread
  // present_gpu uses a real camera (not a degenerate default extent).
  browser_->fit_map_extent();
  browser_->push_shared_extent();
  wire_map_scene();
  attach_viewports();
  browser_->scene3d()->bind_contents(
      browser_->map_session(), map_scene_ ? map_scene_->view_id() : 0);
  browser_->pull_orbit_extent();
  if (browser_->map_session()) {
    browser_->map_session()->SetObserver(browser_);
  }
  attach_hwnd_gestures();
  wire_catalog();
  wire_edit_feedback();
  // Re-fit after HWND sizes settle (layout may change client rect post-attach).
  browser_->fit_map_extent();
  browser_->push_shared_extent();
  // China 3D atmosphere is seeded on first switch to the 3D tab (see
  // switch_map_tab) so init_chrome does not pay DEM/atmosphere cost before
  // the Map pane is interactive.
  sync_status();
  return true;
}

void BrowserView::show_chrome() {
  widget_.show();
}

int BrowserView::run_chrome_loop() {
  return widget_.run_loop();
}

HWND BrowserView::hwnd() const {
  return widget_.hwnd();
}

ui::views::View* BrowserView::contents_view() const {
  ui::views::View* top = widget_.contents_view();
  if (auto* frame = dynamic_cast<ui::views::FrameView*>(top)) {
    return frame->client();
  }
  return top;
}

void BrowserView::build_contents() {
  auto root = std::make_unique<ui::views::View>();
  auto root_box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);

  auto menu = std::make_unique<ui::views::MenuBar>();
  menu->set_preferred_size({0, 28});
  menu_bar_ = menu.get();
  rebuild_menus();

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
  map_tabs->set_preferred_size({0, 0});
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
    if (browser_->plugins() && browser_->plugins()->execute(id)) {
      return;
    }
    browser_->run_tool_command(id);
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
  auto measure_panel = std::make_unique<ui::views::MeasurePanel>();
  measure_panel_ = measure_panel.get();
  auto selection_panel = std::make_unique<ui::views::SelectionPanel>();
  selection_panel_ = selection_panel.get();
  auto layer_properties_panel =
      std::make_unique<ui::views::LayerPropertiesPanel>();
  layer_properties_panel_ = layer_properties_panel.get();
  auto legend_panel = std::make_unique<ui::views::LegendPanel>();
  legend_panel_ = legend_panel.get();
  auto spatial_analysis_panel =
      std::make_unique<ui::views::SpatialAnalysisPanel>();
  spatial_analysis_panel_ = spatial_analysis_panel.get();
  auto atmosphere_panel = std::make_unique<ui::views::AtmospherePanel>();
  atmosphere_panel_ = atmosphere_panel.get();
  auto processing_panel = std::make_unique<ui::views::ProcessingPanel>();
  processing_panel_ = processing_panel.get();

  auto inspector = std::make_unique<ui::views::TabStrip>();
  inspector->set_preferred_size({0, 220});
  feature_info_tab_ = inspector->add_tab("FeatureInfo", std::move(feature_info));
  inspector->add_tab("AttributeTable", std::move(attribute_table));
  measure_tab_ = inspector->add_tab("Measure", std::move(measure_panel));
  selection_tab_ = inspector->add_tab("Selection", std::move(selection_panel));
  layer_props_tab_ =
      inspector->add_tab("LayerProps", std::move(layer_properties_panel));
  legend_tab_ = inspector->add_tab("Legend", std::move(legend_panel));
  spatial_analysis_tab_ =
      inspector->add_tab("Analysis", std::move(spatial_analysis_panel));
  processing_tab_ =
      inspector->add_tab("Processing", std::move(processing_panel));
  inspector->add_tab("Atmosphere", std::move(atmosphere_panel));
  inspector_tabs_ = inspector.get();
  wire_atmosphere_panel();
  wire_processing_panel();
  wire_measure_panel();
  wire_selection_panel();
  wire_layer_properties_panel();
  wire_legend_panel();
  wire_spatial_analysis_panel();

  auto columns = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kVertical);
  // Flex primary: preferred 0 so main_split seeds columns vs tools correctly
  // (non-zero Splitter default 200 would pin the map to 200px when tools are
  // preference-0 / collapsed).
  columns->set_preferred_size({0, 0});
  columns->add_child(std::move(work));
  columns->add_child(std::move(inspector));

  auto diagnostic_tools = ui::views::make_diagnostic_tools_panel();
  diagnostic_tools_ = diagnostic_tools.get();
  wire_debug_console();

  auto main_split = std::make_unique<ui::views::Splitter>(
      ui::views::Splitter::Orientation::kVertical);
  main_split->set_preferred_size({0, 0});
  main_split->add_child(std::move(columns));
  main_split->add_child(std::move(diagnostic_tools));

  auto status = std::make_unique<ui::views::StatusBar>();
  status->set_preferred_size({0, 24});
  status_bar_ = status.get();

  root_box->set_flex_for_view(main_split.get(), 1);
  root->set_layout_manager(std::move(root_box));
  root->add_child(std::move(menu));
  root->add_child(std::move(main_split));
  root->add_child(std::move(status));

  auto frame = std::make_unique<ui::views::FrameView>();
  frame->set_title("SmartGIS Views");
  frame->set_can_maximize(true);
  frame->set_client(std::move(root));
  widget_.set_contents_view(std::move(frame));
}

void BrowserView::wire_catalog() {
  if (!catalog_ || !catalog_->layer_tree()) {
    return;
  }
  sync_catalog_from_scene();
  catalog_->set_source_names({"Memory"});
  catalog_->set_map_docs({{"map.untitled", "", "Untitled map", false}});
  catalog_->set_command(
      [this](const std::string& id) { browser_->on_catalog_command(id); });
  catalog_->layer_tree()->set_visible_changed(
      [this](const std::string& id, bool visible) {
        browser_->document()->set_layer_visible(id, visible);
        content::MapContents* session = active_map()
                                            ? active_map()->map_contents()
                                            : browser_->map_session();
        detail::catalog_call(
            session, std::string("{\"op\":\"set_visible\",\"id\":\"") +
                         detail::json_escape(id) + "\",\"visible\":" +
                         (visible ? "true" : "false") + "}");
        invalidate_map_overlays();
        sync_inspectors_from_scene();
        if (status_bar_) {
          status_bar_->set_message(std::string("Layer ") + id +
                                   (visible ? ": visible" : ": hidden"));
        }
      });
  catalog_->layer_tree()->set_selection_changed([this](const std::string& id) {
    browser_->document()->select_layer(id);
    content::MapContents* session = active_map() ? active_map()->map_contents()
                                                 : browser_->map_session();
    detail::catalog_call(session,
                         std::string("{\"op\":\"select_layer\",\"id\":\"") +
                             detail::json_escape(id) + "\"}");
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    if (status_bar_) {
      status_bar_->set_message("Active layer: " + id);
    }
  });
}

void BrowserView::sync_catalog_from_scene() {
  if (!catalog_) {
    return;
  }
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  for (const content::LayerDesc& d : browser_->document()->layer_descs()) {
    ui::views::LayerTree::LayerDesc row;
    row.id = d.id;
    row.name = d.name;
    row.visible = d.visible;
    row.active = d.active;
    layers.push_back(std::move(row));
  }
  catalog_->populate_layers(layers);
}

bool BrowserView::scene3d_tab_active() const {
  return map_tabs_ && map_tabs_->active() == 2;
}

void BrowserView::on_map_right_click(HWND map_hwnd, int view_x, int view_y) {
  if (!map_hwnd || !browser_) {
    return;
  }
  // TrackPopupMenu pumps messages; calling it from the map HWND subclass
  // during WM_RBUTTONUP re-enters the gesture/input stack and can AV. Defer
  // one tick like schedule_menu_rebuild (MapLibre-like: RMB = menu only).
  pending_map_menu_hwnd_ = map_hwnd;
  pending_map_menu_x_ = view_x;
  pending_map_menu_y_ = view_y;
  HWND owner = hwnd();
  if (!owner) {
    show_pending_map_context_menu();
    return;
  }
  constexpr UINT_PTR kMapCtxTimer = 0x4D4354u;  // 'MCT'
  SetPropW(owner, L"SmtMapCtxBrowser", reinterpret_cast<HANDLE>(this));
  KillTimer(owner, kMapCtxTimer);
  SetTimer(owner, kMapCtxTimer, 1,
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"SmtMapCtxBrowser"));
             if (self) {
               self->show_pending_map_context_menu();
             }
           });
}

void BrowserView::show_pending_map_context_menu() {
  HWND map_hwnd = pending_map_menu_hwnd_;
  const int view_x = pending_map_menu_x_;
  const int view_y = pending_map_menu_y_;
  pending_map_menu_hwnd_ = nullptr;
  if (!map_hwnd || !IsWindow(map_hwnd) || !browser_) {
    return;
  }
  // Headless / self-test: skip modal popup (would hang the pump).
  if (GetEnvironmentVariableA("SMT_SKIP_MAP_CONTEXT_MENU", nullptr, 0) > 0) {
    return;
  }
  std::vector<std::string> labels;
  collect_bookmark_labels(browser_, &labels);
  const std::vector<ui::views::MenuItem> items = navigation_menu_items(
      labels, [this, view_x, view_y](std::string_view id, int index) {
        browser_->on_view_command(id, index, true, view_x, view_y);
      });
  POINT pt{view_x, view_y};
  ClientToScreen(map_hwnd, &pt);
  ui::views::show_context_menu(map_hwnd, ui::views::Point{pt.x, pt.y}, items);
}

void BrowserView::schedule_menu_rebuild() {
  HWND owner = hwnd();
  if (!owner) {
    rebuild_menus();
    return;
  }
  constexpr UINT_PTR kMenuTimer = 0x4D4E55u;
  SetPropW(owner, L"SmtMenuBrowser", reinterpret_cast<HANDLE>(this));
  KillTimer(owner, kMenuTimer);
  SetTimer(owner, kMenuTimer, 1,
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"SmtMenuBrowser"));
             if (self) {
               self->rebuild_menus();
             }
           });
}

void BrowserView::rebuild_menus() {
  if (!menu_bar_ || !browser_) {
    return;
  }
  std::vector<std::string> labels;
  collect_bookmark_labels(browser_, &labels);
  ShellMenus menus = build_shell_menus(
      labels, [this](std::string_view id, int index) {
        if (id == "shell.open") {
          browser_->on_open();
          return;
        }
        if (id == "shell.save") {
          browser_->on_save_document();
          return;
        }
        if (id == "shell.export") {
          browser_->on_export_document();
          return;
        }
        if (id == "shell.exit") {
          on_exit();
          return;
        }
        if (id == "view.debug_console") {
          toggle_debug_console();
          return;
        }
        if (id == "view.theme.dark") {
          ui::views::ThemeService::get().set_theme("dark");
          return;
        }
        if (id == "view.theme.light") {
          ui::views::ThemeService::get().set_theme("light");
          return;
        }
        if (id == "view.preferences") {
          std::vector<std::string> labels;
          std::vector<std::string> ids;
          for (const auto& pack : ui::views::ThemeService::get().packs()) {
            labels.push_back(pack.label);
            ids.push_back(pack.id);
          }
          std::string chosen;
          if (ui::views::SelectOneDialog::run(widget_.hwnd(), labels,
                                              &chosen)) {
            for (size_t i = 0; i < labels.size(); ++i) {
              if (labels[i] == chosen) {
                ui::views::ThemeService::get().set_theme(ids[i]);
                break;
              }
            }
          }
          return;
        }
        if (id.starts_with("catalog.")) {
          browser_->on_catalog_command(std::string(id));
          return;
        }
        browser_->on_view_command(id, index, false, 0, 0);
      });
  menu_bar_->clear();
  menu_bar_->add_menu("File", std::move(menus.file));
  menu_bar_->add_menu("Edit", std::move(menus.edit));
  menu_bar_->add_menu("View", std::move(menus.view));
  menu_bar_->add_menu("Layer", std::move(menus.layer));
}

void BrowserView::populate_ambox() {
  if (!ambox_) {
    return;
  }
  std::vector<tool::CommandCatalog*> catalogs;
  if (browser_->edit_host() && browser_->edit_host()->workspace()) {
    catalogs.push_back(&browser_->edit_host()->workspace()->catalog());
  }
  if (browser_->plugins()) {
    if (tool::CommandCatalog* plugin_catalog = browser_->plugins()->commands()) {
      catalogs.push_back(plugin_catalog);
    }
  }
  std::vector<ui::views::AmboxView::Group> plugin_groups;
  if (browser_->plugins()) {
    plugin_groups = enabled_plugin_groups(browser_->plugins()->registry(),
                                          browser_->plugins()->host());
  }
  ambox_->populate_from_commands(catalogs, std::move(plugin_groups));
}

void BrowserView::set_status_message(const std::string& text) {
  if (status_bar_) {
    status_bar_->set_message(text);
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
  if (browser_->plugins()) {
    browser_->plugins()->show_manager(widget_.hwnd());
    populate_ambox();
  }
}

void BrowserView::on_processing() {
  show_inspector_tab_index(spatial_analysis_tab_ >= 0 ? spatial_analysis_tab_
                                                      : processing_tab_);
  if (spatial_analysis_panel_ &&
      !spatial_analysis_panel_->selected_id().empty()) {
    set_status_message(std::string("Analysis: ") +
                       spatial_analysis_panel_->selected_id());
  } else if (processing_panel_ && !processing_panel_->selected_id().empty()) {
    set_status_message(std::string("Processing: ") +
                       processing_panel_->selected_id());
  } else {
    set_status_message("Spatial analysis toolbox");
  }
}

void BrowserView::show_inspector_tab_index(int index) {
  if (inspector_tabs_ && index >= 0) {
    inspector_tabs_->set_active(index);
  }
}

void BrowserView::sync_status() {
  ui::views::MapViewport* pane = active_map();
  if (!status_bar_ || !pane) {
    return;
  }
  status_bar_->set_status(detail::wide_to_utf8(pane->status_text()));
}

void BrowserView::select_map_tab(int index) {
  switch_map_tab(index);
}

void BrowserView::show_feature_info_tab() {
  show_inspector_tab_index(feature_info_tab_ >= 0 ? feature_info_tab_ : 0);
}

void BrowserView::schedule_overlay_full_redraw() {
  HWND h = nullptr;
  if (ui::views::MapViewport* pane = active_map()) {
    h = pane->native_view();
  }
  if (!h) {
    h = hwnd();
  }
  if (!h) {
    return;
  }
  constexpr UINT_PTR kId = 0x424C54u;
  SetPropW(h, L"SmtBlitBrowser", reinterpret_cast<HANDLE>(this));
  KillTimer(h, kId);
  SetTimer(h, kId, static_cast<UINT>(tool::kBlitDebounceMs),
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"SmtBlitBrowser"));
             if (self && self->browser_) {
               self->browser_->commit_blit_preview();
             } else {
               InvalidateRect(timer_hwnd, nullptr, FALSE);
             }
           });
}

}  // namespace app
