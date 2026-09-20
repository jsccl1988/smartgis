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

#include "app/views/blit_frame_cache.h"
#include "app/views/app_commands.h"
#include "app/views/map_host_extent.h"
#include "app/views/plugin_chrome.h"
#include "content/public/catalog_layers.h"
#include "content/public/events.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/map_widget_host_view.h"
#include "content/public/view_host.h"
#include "gis/atmosphere/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/edit/edit_session.h"
#include "gis/tile/tile_map_layer.h"
#include "tool/camera_nav.h"
#include "tool/command.h"
#include "tool/gestures.h"
#include "tool/workspace.h"
#include "ui/views/dialogs/add_basemap_dialog.h"
#include "ui/views/gis/ambox_view.h"
#include "ui/views/gis/atmosphere_panel.h"
#include "ui/views/dialogs/att_struct_dialog.h"
#include "ui/views/gis/attribute_table.h"
#include "ui/views/gis/catalog_view.h"
#include "ui/views/dialogs/create_datasource_dialog.h"
#include "ui/views/dialogs/create_layer_dialog.h"
#include "ui/views/dialogs/create_map_dialog.h"
#include "ui/views/gis/feature_info.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/views/gis/layer_tree.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu_bar.h"
#include "ui/views/kernel/splitter.h"
#include "ui/views/gis/status_bar.h"
#include "ui/views/primitives/tab_strip.h"
#include "ui/views/kernel/view.h"

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

// True when ContentMapView has a shared DIB generation (GPU demo DEM / frame).
bool viewport_has_shared_frame(ui::views::MapViewport* pane) {
  if (!pane ||
      pane->attach_mode() !=
          ui::views::MapViewport::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}

}  // namespace detail

BrowserView::BrowserView() = default;

BrowserView::~BrowserView() {
  // Stop present timers / clear HWND userdata before the Widget tears down
  // the view tree (avoids heap corruption from late WM_TIMER/WM_PAINT).
  edit_gestures_.detach();
  data_gestures_.detach();
  scene_gestures_.detach();
  if (map_session_) {
    map_session_->SetObserver(nullptr);
  }
  scene3d_.abandon_mesh();
  if (map_edit_) {
    map_edit_->detach();
  }
  if (map_data_) {
    map_data_->detach();
  }
  if (map_scene_) {
    map_scene_->detach();
  }
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
  document_.seed_default();
  scene3d_.bind_map(&document_);
  wire_map_scene();
  attach_viewports();
  scene3d_.bind_contents(map_session_.get(),
                         map_scene_ ? map_scene_->view_id() : 0);
  if (map_session_) {
    map_session_->SetObserver(this);
  }
  attach_hwnd_gestures();
  wire_catalog();
  wire_edit_feedback();
  // seed_default fits to 800x600; re-fit once HWND sizes are real.
  fit_map_extent();
  push_shared_extent();
  sync_status();
  return true;
}

void BrowserView::show() {
  widget_.show();
  // HWND client size is reliable after ShowWindow.
  fit_map_extent();
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
  menu->add_item("Map", [this]() { switch_map_tab(0); });
  menu->add_item("Data", [this]() { switch_map_tab(1); });
  menu->add_item("3D", [this]() { switch_map_tab(2); });
  menu->add_item("Select", [this]() { run_tool_command("selection.point"); });
  menu->add_item("Draw", [this]() { run_tool_command("edit.append.point"); });
  menu->add_item("Clear", [this]() { run_tool_command("selection.clear"); });
  menu->add_item("Undo", [this]() { run_tool_command("edit.undo"); });
  menu->add_item("RHI", [this]() { run_tool_command("view.backend.rhi"); });
  menu->add_item("MapLibre", [this]() { run_tool_command("view.backend.maplibre"); });
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
  // Width 0 鈫?Splitter treats map tabs as the flex pane (catalog stays ~240).
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
  auto atmosphere_panel = std::make_unique<ui::views::AtmospherePanel>();
  atmosphere_panel_ = atmosphere_panel.get();

  auto inspector = std::make_unique<ui::views::TabStrip>();
  inspector->set_preferred_size({0, 180});
  inspector->add_tab("FeatureInfo", std::move(feature_info));
  inspector->add_tab("AttributeTable", std::move(attribute_table));
  inspector->add_tab("Atmosphere", std::move(atmosphere_panel));
  wire_atmosphere_panel();

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
  // Ensure every map tab page has a real client rect before OpenView/Resize
  // (inactive tabs used to keep 0x0 bounds).
  widget_.layout_contents();

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
  widget_.layout_contents();
  // After layout, HWND clients are real 鈥?push surface size so GPU demo
  // frames are not stuck at the 64脳64 attach fallback (avoids StretchDIBits
  // pixelation).
  for (ui::views::MapViewport* pane : {map_edit_, map_data_, map_scene_}) {
    if (!pane || !pane->native_view()) {
      continue;
    }
    pane->sync_native_bounds();
    RECT rc = {};
    GetClientRect(pane->native_view(), &rc);
    if (rc.right > 0 && rc.bottom > 0) {
      // WM_SIZE may have raced before the view had final bounds.
      SendMessageW(pane->native_view(), WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(rc.right, rc.bottom));
    }
  }
}

void BrowserView::wire_catalog() {
  if (!catalog_ || !catalog_->layer_tree()) {
    return;
  }
  sync_catalog_from_scene();
  catalog_->set_source_names({"Memory"});
  catalog_->set_map_docs(
      {{"map.untitled", "", "Untitled map", false}});
  catalog_->set_command(
      [this](const std::string& id) { on_catalog_command(id); });
  catalog_->layer_tree()->set_visible_changed(
      [this](const std::string& id, bool visible) {
        document_.set_layer_visible(id, visible);
        content::MapContents* session =
            active_map() ? active_map()->map_contents() : map_session_.get();
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
    document_.select_layer(id);
    content::MapContents* session =
        active_map() ? active_map()->map_contents() : map_session_.get();
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
  KillTimer(h, kId);
  SetTimer(h, kId, static_cast<UINT>(tool::kBlitDebounceMs),
           [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(hwnd, id);
             InvalidateRect(hwnd, nullptr, FALSE);
           });
}

void BrowserView::wire_map_scene() {
  auto paint2d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (blit_.in_preview() && blit_.present(hdc, w, h)) {
      return;
    }
    document_.paint(hdc, w, h, true);
    blit_.capture(hdc, w, h);
  };
  auto paint3d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return;
    }
    const auto mode = map_scene_ ? map_scene_->attach_mode()
                                 : ui::views::MapViewport::AttachMode::kNone;
    // Orbitable SoT = Scene3dController GDI DEM (elevation + labels + compass).
    // ContentMapView DIB is a static GPU demo underlay — never leave it as the
    // only frame (that showed wireframe / solid olive and ignored orbit).
    // FlyCube + present_gpu → HUD only.
    const bool flycube = mode == ui::views::MapViewport::AttachMode::kFlyCube;
    const bool gpu_ok =
        flycube && map_scene_ && map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      scene3d_.paint_hud(hdc, w, h);
    } else {
      scene3d_.paint(hdc, w, h, /*fill_background=*/true);
    }
  };
  if (map_edit_) {
    map_edit_->set_overlay_paint(paint2d);
  }
  if (map_data_) {
    map_data_->set_overlay_paint(paint2d);
  }
  if (map_scene_) {
    map_scene_->set_overlay_paint(paint3d);
    map_scene_->set_gpu_present(
        [this](void* device, uint32_t w, uint32_t h) -> bool {
          return scene3d_.present_gpu(
              static_cast<render::rhi::Device*>(device), w, h);
        });
  }
  auto on_draft = [this](const tool::Draft& draft) { handle_draft(draft); };
  if (edit_host_ && edit_host_->workspace()) {
    edit_host_->workspace()->set_draft_observer(on_draft);
  }
  if (data_host_ && data_host_->workspace()) {
    data_host_->workspace()->set_draft_observer(on_draft);
  }
  if (scene_host_ && scene_host_->workspace()) {
    scene_host_->workspace()->set_draft_observer(on_draft);
  }
  sync_inspectors_from_scene();
}

void BrowserView::wire_atmosphere_panel() {
  if (!atmosphere_panel_) {
    return;
  }
  atmosphere_panel_->set_time_range(0.0, 3600.0);
  atmosphere_panel_->set_time_sec(scene3d_.time_sec());
  atmosphere_panel_->set_ocean_checked(false);
  atmosphere_panel_->set_cloud_checked(false);
  atmosphere_panel_->set_wind_checked(false);

  atmosphere_panel_->set_time_change([this](double t) {
    scene3d_.set_time_sec(t);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_ocean_change([this](bool on) {
    if (on) {
      const auto* env = scene3d_.atmosphere();
      if (!env || env->field_store().layer_count() == 0) {
        scene3d_.seed_atmosphere_procedural();
      }
    }
    scene3d_.set_ocean_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_cloud_change([this](bool on) {
    if (on) {
      const auto* env = scene3d_.atmosphere();
      if (!env || env->field_store().layer_count() == 0) {
        scene3d_.seed_atmosphere_procedural();
      }
    }
    scene3d_.set_cloud_enabled(on);
    invalidate_map_overlays();
  });
  atmosphere_panel_->set_wind_change([this](bool on) {
    scene3d_.set_wind_overlay_enabled(on);
    invalidate_map_overlays();
  });
}

bool BrowserView::apply_atmosphere_fields(std::string_view spec) {
  const bool ok = scene3d_.load_atmosphere_fields(spec);
  if (ok && atmosphere_panel_) {
    double t_min = 0.0;
    double t_max = 3600.0;
    if (auto* env = scene3d_.atmosphere()) {
      static const gis::atmosphere::FieldChannel kRangeOrder[] = {
          gis::atmosphere::FieldChannel::kWaveHs,
          gis::atmosphere::FieldChannel::kCloudCover,
          gis::atmosphere::FieldChannel::kWindU,
          gis::atmosphere::FieldChannel::kWindV,
          gis::atmosphere::FieldChannel::kWaveDir,
          gis::atmosphere::FieldChannel::kCloudBase,
          gis::atmosphere::FieldChannel::kCloudTop,
          gis::atmosphere::FieldChannel::kSeaMask,
      };
      for (gis::atmosphere::FieldChannel ch : kRangeOrder) {
        if (env->timed_field_range(ch, &t_min, &t_max)) {
          break;
        }
      }
    }
    if (t_max < t_min) {
      std::swap(t_min, t_max);
    }
    if (t_max <= t_min) {
      t_max = t_min + 1.0;
    }
    atmosphere_panel_->set_time_range(t_min, t_max);
    atmosphere_panel_->set_time_sec(scene3d_.time_sec());
  }
  if (ok) {
    invalidate_map_overlays();
    set_status_message("Atmosphere fields loaded");
  } else {
    set_status_message("Atmosphere fields load failed");
  }
  return ok;
}

void BrowserView::sync_catalog_from_scene() {
  if (!catalog_) {
    return;
  }
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  for (const content::LayerDesc& d : document_.layer_descs()) {
    ui::views::LayerTree::LayerDesc row;
    row.id = d.id;
    row.name = d.name;
    row.visible = d.visible;
    row.active = d.active;
    layers.push_back(std::move(row));
  }
  catalog_->populate_layers(layers);
}

void BrowserView::sync_inspectors_from_scene() {
  if (attribute_table_) {
    std::vector<std::string> cols;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> tokens;
    document_.fill_attribute_rows(&cols, &rows, &tokens);
    attribute_table_->set_columns(cols);
    attribute_table_->set_rows(rows);
    attribute_table_->set_row_tokens(std::move(tokens));
  }
  if (feature_info_) {
    if (const MapScene::Feature* f = document_.selected_feature()) {
      feature_info_->set_feature_id(MapScene::feature_token(f->id));
      std::vector<std::pair<std::string, std::string>> pairs;
      document_.fill_feature_info_fields(*f, &pairs);
      std::vector<ui::views::FeatureInfo::Field> fields;
      fields.reserve(pairs.size());
      for (auto& p : pairs) {
        fields.push_back({std::move(p.first), std::move(p.second)});
      }
      feature_info_->set_fields(fields);
    }
  }
}

void BrowserView::invalidate_map_overlays() {
  if (map_edit_) {
    map_edit_->invalidate_native();
  }
  if (map_data_) {
    map_data_->invalidate_native();
  }
  if (map_scene_) {
    map_scene_->invalidate_native();
  }
}

void BrowserView::fit_map_extent() {
  int w = 800;
  int h = 600;
  if (ui::views::MapViewport* pane = active_map()) {
    if (HWND hwnd = pane->native_view()) {
      RECT rc = {};
      GetClientRect(hwnd, &rc);
      if (rc.right > 32) {
        w = rc.right;
      }
      if (rc.bottom > 32) {
        h = rc.bottom;
      }
    }
  }
  document_.fit_extent(w, h);
  scene3d_.apply_world_extent(document_.world_extent());
  push_shared_extent();
  invalidate_map_overlays();
  if (status_bar_) {
    if (document_.last_open_was_ogr()) {
      status_bar_->set_crs_text(
          document_.has_china_extent() ? "EPSG:4326 (China)" : "EPSG:4326");
    } else {
      status_bar_->set_crs_text("local");
    }
    status_bar_->set_message(
        "Layers: " + std::to_string(document_.layer_count()) +
        " Features: " + std::to_string(document_.feature_count()));
  }
}

void BrowserView::handle_draft(const tool::Draft& draft) {
  content::ViewHost* host = active_view_host();
  tool::Interaction* cur =
      host && host->workspace() ? host->workspace()->stack().current()
                                : nullptr;
  const char* tool_id = cur ? cur->id() : "";

  if (tool_id && std::strncmp(tool_id, "view3d.", 7) == 0) {
    scene3d_.apply_draft(draft);
    forward_draft_to_contents(draft);
    if (map_scene_) {
      map_scene_->invalidate_native();
    }
    return;
  }

  // Always-on horizontal wheel / two-finger pan drafts (before select/draw).
  if (draft.kind == tool::DraftKind::kRect &&
      tool::draft_flags::is_touch_pan(draft.flags) &&
      draft.points.size() >= 2 && tool::is_navigate_tool(tool_id)) {
    const int dx = draft.points.back().x_px - draft.points.front().x_px;
    const int dy = draft.points.back().y_px - draft.points.front().y_px;
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_pan(vw, vh, dx, dy);
    document_.apply_pan(dx, dy);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
    return;
  }

  if (tool_id && std::strncmp(tool_id, "select.", 7) == 0) {
    if (draft.points.empty()) {
      return;
    }
    RECT rc = {};
    if (ui::views::MapViewport* pane = active_map()) {
      if (HWND hwnd = pane->native_view()) {
        GetClientRect(hwnd, &rc);
      }
    }
    const MapScene::Feature* hit = document_.hit_test(
        draft.points.front().x_px, draft.points.front().y_px, rc.right,
        rc.bottom);
    if (hit) {
      set_status_message("Selected " + MapScene::feature_token(hit->id));
      if (feature_info_) {
        feature_info_->set_feature_id(MapScene::feature_token(hit->id));
        std::vector<std::pair<std::string, std::string>> pairs;
        document_.fill_feature_info_fields(*hit, &pairs);
        std::vector<ui::views::FeatureInfo::Field> fields;
        for (auto& p : pairs) {
          fields.push_back({std::move(p.first), std::move(p.second)});
        }
        feature_info_->set_fields(fields);
      }
    } else {
      document_.clear_selection();
      if (feature_info_) {
        feature_info_->clear();
      }
      set_status_message("Selection cleared");
    }
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    return;
  }

  if (tool_id && std::strncmp(tool_id, "draw.", 5) == 0) {
    document_.append_from_draft(draft, tool_id);
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    return;
  }

  if (tool_id && std::strcmp(tool_id, "view.pan") == 0 &&
      draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    const int dx = draft.points.back().x_px - draft.points.front().x_px;
    const int dy = draft.points.back().y_px - draft.points.front().y_px;
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_pan(vw, vh, dx, dy);
    document_.apply_pan(dx, dy);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
    return;
  }

  if (tool_id && std::strcmp(tool_id, "view.zoom_in") == 0 &&
      !draft.points.empty()) {
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_zoom(vw, vh, draft.points.front().x_px,
                     draft.points.front().y_px, 1.25);
    document_.apply_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, 1.25);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
    return;
  }
  if (tool_id && std::strcmp(tool_id, "view.zoom_out") == 0 &&
      !draft.points.empty()) {
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_zoom(vw, vh, draft.points.front().x_px,
                     draft.points.front().y_px, 0.8);
    document_.apply_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, 0.8);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
    return;
  }

  // Always-on map UX: wheel / double-click zoom toward cursor (not view center).
  if (draft.kind == tool::DraftKind::kWheel && !draft.points.empty() &&
      tool::is_navigate_tool(tool_id)) {
    const double factor = tool::wheel_zoom_factor(draft.wheel);
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_zoom(vw, vh, draft.points.front().x_px,
                     draft.points.front().y_px, factor);
    document_.apply_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, factor);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
    return;
  }
  if (draft.kind == tool::DraftKind::kPoint &&
      tool::is_navigate_tool(tool_id) && !draft.points.empty()) {
    int vw = 800;
    int vh = 600;
    active_view_size(&vw, &vh);
    blit_.begin_zoom(vw, vh, draft.points.front().x_px,
                     draft.points.front().y_px, 1.25);
    document_.apply_zoom_at(draft.points.front().x_px,
                            draft.points.front().y_px, 1.25);
    forward_draft_to_contents(draft);
    push_shared_extent();
    invalidate_map_overlays();
    schedule_overlay_full_redraw();
  }
}

void BrowserView::wire_edit_feedback() {
  if (!edit_host_ || !edit_host_->events()) {
    return;
  }
  selection_sub_ = edit_host_->events()->subscribe<content::SelectionChanged>(
      [this](const content::SelectionChanged& ev) {
        if (ev.ids.empty()) {
          // Prefer MapScene hit-test result from draft_observer; only clear
          // when the workspace explicitly cleared selection.
          if (!document_.selected_feature()) {
            set_status_message("Selection cleared");
            if (feature_info_) {
              feature_info_->clear();
            }
          }
          return;
        }
        set_status_message("Selected " + std::to_string(ev.ids.size()) +
                           " feature(s)");
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
        sync_inspectors_from_scene();
        invalidate_map_overlays();
      });
  extent_sub_ = edit_host_->events()->subscribe<content::ExtentChanged>(
      [this](const content::ExtentChanged& ev) {
        OnExtentChanged(ev.view_id, ev.extent);
      });

  if (attribute_table_) {
    attribute_table_->set_on_cell_commit(
        [this](const std::string& feature_token, const std::string& field,
               const std::string& value) {
          if (!document_.update_feature_field(feature_token, field, value)) {
            set_status_message("Attribute edit failed: " + field);
            return false;
          }
          content::ViewHost* host = active_view_host();
          if (host && host->edits()) {
            gis::FeatureMutation mutation;
            mutation.op = gis::EditOp::kModify;
            mutation.id = MapScene::feature_id_from_token(feature_token);
            host->edits()->commit(mutation);
          }
          set_status_message("Updated " + field + "=" + value);
          sync_inspectors_from_scene();
          invalidate_map_overlays();
          return true;
        });
    attribute_table_->set_selected([this](int row) {
      if (!attribute_table_) {
        return;
      }
      const std::string& token = attribute_table_->row_token(row);
      if (token.empty()) {
        return;
      }
      const content::FeatureId id = MapScene::feature_id_from_token(token);
      if (!document_.select_feature(id)) {
        return;
      }
      sync_inspectors_from_scene();
      invalidate_map_overlays();
      set_status_message("Selected " + token);
    });
  }
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
  } else if (id == "full") {
    id = "view.full";
  }
  const uint32_t view_id = active_map() ? active_map()->view_id() : 0;
  if (!host->execute(id, view_id)) {
    set_status_message("Unknown tool " + id);
    return false;
  }
  if (id == "view.full" || id == "view3d.full") {
    fit_map_extent();
    set_status_message("View full extent");
    return true;
  }
  if (id == "view.backend.rhi" || id == "view.backend.maplibre") {
    const uint32_t kind = (id == "view.backend.maplibre") ? 1u : 0u;
    if (map_session_) {
      map_session_->SetRenderBackend(kind);
    }
    set_status_message(kind ? "Render: MapLibre (Track A)"
                            : "Render: RHI (Track B)");
    return true;
  }
  if (id == "selection.clear") {
    document_.clear_selection();
    if (feature_info_) {
      feature_info_->clear();
    }
    sync_inspectors_from_scene();
    invalidate_map_overlays();
    set_status_message("Selection cleared");
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
    if (ui::views::CreateLayerDialog::run(hwnd, &out)) {
      if (document_.create_layer(out.name, out.geometry_type)) {
        sync_catalog_from_scene();
        sync_inspectors_from_scene();
        invalidate_map_overlays();
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
    gis::MapLayer layer;
    if (out.kind == "wmts") {
      layer = gis::tile::make_wmts_map_layer(out.url);
    } else {
      layer = gis::tile::make_xyz_map_layer(out.url);
    }
    if (layer.leftover() == nullptr) {
      status("Basemap URL rejected");
      return;
    }
    const std::string name =
        out.name.empty() ? std::string("Basemap") : out.name;
    document_.create_layer(name, out.kind);
    sync_catalog_from_scene();
    sync_inspectors_from_scene();
    invalidate_map_overlays();
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
        !text.empty()) {
      if (command_id == "catalog.layer.append") {
        document_.create_layer(text, "point");
        sync_catalog_from_scene();
        sync_inspectors_from_scene();
        invalidate_map_overlays();
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
      const bool ogr_ok = document_.open_path(file.path);
      sync_catalog_from_scene();
      sync_inspectors_from_scene();
      fit_map_extent();
      refresh();
      if (ogr_ok) {
        status("OGR opened " + file.path + " (" +
               std::to_string(document_.layer_count()) + " layers, " +
               std::to_string(document_.feature_count()) + " features)");
      } else {
        status("Opened (sample fallback) " + file.path);
      }
    }
    return;
  }
  if (command_id == "catalog.map.save" || command_id == "catalog.map.save_as") {
    const ui::views::FilePickerResult file =
        ui::views::pick_save_file(hwnd, L"Map\0*.smtmap\0All\0*.*\0");
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
    if (!id.empty() && document_.remove_layer(id)) {
      sync_catalog_from_scene();
      sync_inspectors_from_scene();
      invalidate_map_overlays();
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
        catalog_ && catalog_->layer_tree()
            ? catalog_->layer_tree()->selected_id()
            : std::string();
    const int delta = (command_id == "catalog.layer.move_up") ? -1 : 1;
    if (!id.empty() && document_.move_layer(id, delta)) {
      sync_catalog_from_scene();
      invalidate_map_overlays();
      status(delta < 0 ? "Layer moved up" : "Layer moved down");
    }
    return;
  }
  if (command_id == "catalog.layer.active") {
    const std::string id =
        catalog_ && catalog_->layer_tree()
            ? catalog_->layer_tree()->selected_id()
            : std::string();
    if (!id.empty() && document_.select_layer(id)) {
      sync_catalog_from_scene();
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
                         catalog_ && catalog_->layer_tree()
                             ? catalog_->layer_tree()->selected_id()
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
  document_.open_path(cmd.path);
  sync_catalog_from_scene();
  sync_inspectors_from_scene();
  fit_map_extent();
  if (catalog_) {
    catalog_->set_map_docs(
        {{cmd.path, "", detail::path_stem(cmd.path), false}});
    catalog_->set_source_names({detail::path_stem(cmd.path)});
  }
  if (content::ViewHost* host = active_view_host()) {
    const uint32_t view_id = pane ? pane->view_id() : 0;
    host->execute("view.refresh", view_id);
  }
  if (status_bar_) {
    const char* kind = document_.last_open_was_ogr() ? "OGR" : "sample";
    status_bar_->set_status(std::string("Opened (") + kind + "): " + cmd.path +
                            " (" + std::to_string(document_.layer_count()) +
                            " layers, " +
                            std::to_string(document_.feature_count()) +
                            " features)");
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
  // TabStrip show/hides native map HWNDs via View::set_visible; also force
  // Win32 visibility so self-test / rapid tab switches cannot leave the active
  // pane hidden when sync_native_bounds skips a no-op SetWindowPos.
  auto sync_hwnd = [](ui::views::MapViewport* pane, bool show) {
    if (!pane) {
      return;
    }
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        ShowWindow(hwnd, show ? SW_SHOWNA : SW_HIDE);
      }
    }
  };
  sync_hwnd(map_edit_, i == 0);
  sync_hwnd(map_data_, i == 1);
  sync_hwnd(map_scene_, i == 2);
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
      // WinUI show_kind parity: re-bind Scene3d view id + China extent so DEM
      // seed / present_gpu / GDI paint share the same world frame.
      if (map_scene_) {
        scene3d_.bind_contents(map_session_.get(), map_scene_->view_id());
      }
      // Recover from edge-on / over-zoomed orbit (thin green DEM strip).
      scene3d_.reset();
      push_shared_extent();
      if (map_scene_) {
        map_scene_->invalidate_native();
      }
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

void BrowserView::active_view_size(int* w, int* h) const {
  int width = 800;
  int height = 600;
  if (ui::views::MapViewport* pane = active_map()) {
    if (HWND hwnd = pane->native_view()) {
      RECT rc = {};
      GetClientRect(hwnd, &rc);
      if (rc.right > 32) {
        width = rc.right;
      }
      if (rc.bottom > 32) {
        height = rc.bottom;
      }
    }
  }
  if (w) {
    *w = width;
  }
  if (h) {
    *h = height;
  }
}

void BrowserView::push_shared_extent() {
  if (syncing_extent_) {
    return;
  }
  int w = 800;
  int h = 600;
  active_view_size(&w, &h);
  content::Extent2 e = document_.view_world_extent(w, h);
  if (!extent_looks_like_china(e)) {
    const content::Extent2 world = document_.world_extent();
    e = extent_looks_like_china(world) ? world : kChinaLonLatExtent;
  }
  scene3d_.apply_world_extent(e);
  if (!map_session_) {
    return;
  }
  syncing_extent_ = true;
  for (ui::views::MapViewport* pane : {map_edit_, map_data_, map_scene_}) {
    if (pane && pane->view_id() != 0) {
      map_session_->SetExtent(pane->view_id(), e);
    }
  }
  scene3d_.push_extent_to_contents();
  syncing_extent_ = false;
}

void BrowserView::OnExtentChanged(uint32_t /*view_id*/,
                                 const content::Extent2& e) {
  if (syncing_extent_ || !extent_nonempty(e)) {
    return;
  }
  int w = 800;
  int h = 600;
  active_view_size(&w, &h);
  syncing_extent_ = true;
  document_.apply_world_extent(e, w, h);
  scene3d_.apply_world_extent(e);
  syncing_extent_ = false;
  invalidate_map_overlays();
}

void BrowserView::forward_draft_to_contents(const tool::Draft& draft) {
  if (!map_session_) {
    return;
  }
  ui::views::MapViewport* pane = active_map();
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
    map_session_->Dispatch(view_id, e);
    return;
  }
  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    e.kind = content::InputEvent::Kind::kLDown;
    e.x_px = draft.points.front().x_px;
    e.y_px = draft.points.front().y_px;
    map_session_->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kMouseMove;
    e.x_px = draft.points.back().x_px;
    e.y_px = draft.points.back().y_px;
    map_session_->Dispatch(view_id, e);
    e.kind = content::InputEvent::Kind::kLUp;
    map_session_->Dispatch(view_id, e);
  }
}

void BrowserView::attach_hwnd_gestures() {
  auto on_pinch = [this](int x, int y, double scale) {
    handle_pinch(x, y, scale);
  };
  auto on_pan = [this](int dx, int dy) { handle_gesture_pan(dx, dy); };
  if (map_edit_ && map_edit_->native_view()) {
    edit_gestures_.attach(map_edit_->native_view(), on_pinch, on_pan);
  }
  if (map_data_ && map_data_->native_view()) {
    data_gestures_.attach(map_data_->native_view(), on_pinch, on_pan);
  }
  if (map_scene_ && map_scene_->native_view()) {
    scene_gestures_.attach(map_scene_->native_view(), on_pinch, on_pan);
  }
}

void BrowserView::handle_pinch(int view_x, int view_y, double scale) {
  int w = 800;
  int h = 600;
  active_view_size(&w, &h);
  const int tab = map_tabs_ ? map_tabs_->active() : 0;
  if (tab == 2) {
    scene3d_.apply_pinch(view_x, view_y, scale, w, h);
    if (map_scene_) {
      map_scene_->invalidate_native();
    }
  } else {
    document_.apply_pinch(view_x, view_y, scale);
  }
  push_shared_extent();
  invalidate_map_overlays();
}

void BrowserView::handle_gesture_pan(int dx_px, int dy_px) {
  if (dx_px == 0 && dy_px == 0) {
    return;
  }
  int vw = 800;
  int vh = 600;
  active_view_size(&vw, &vh);
  const int tab = map_tabs_ ? map_tabs_->active() : 0;
  if (tab == 2) {
    scene3d_.apply_pan(dx_px, dy_px);
    if (map_scene_) {
      map_scene_->invalidate_native();
    }
  } else {
    blit_.begin_pan(vw, vh, dx_px, dy_px);
    document_.apply_pan(dx_px, dy_px);
  }
  push_shared_extent();
  invalidate_map_overlays();
  schedule_overlay_full_redraw();
}

}  // namespace app
