// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/browser_view.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include <windowsx.h>
#include <commctrl.h>

#include "base/core/log.h"
#include "content/browser/input/map_hwnd_gestures.h"
#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/commands/app_commands.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/sample.h"
#include "content/public/map_contents.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "plugin/runtime/host/registry/registry.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/nav/camera_nav.h"
#include "tool/workspace/workspace.h"
#include "app/views/shell/ui/pages/map_pages_chrome.h"
#include "app/views/shell/ui/panels/atmosphere_chrome.h"
#include "app/views/shell/ui/panels/debug_console_chrome.h"
#include "app/views/shell/ui/panels/inspect_chrome.h"
#include "app/views/shell/ui/panels/inspector_sync_chrome.h"
#include "app/views/shell/ui/panels/processing_chrome.h"
#include "app/views/shell/ui/panels/report_panel.h"
#include "app/views/shell/ui/shell_layout_chrome.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
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
#include "ui/gis/analysis/result_playback_panel.h"
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
// and CRT abort() 闁?seen at BrowserView::rebuild_menus during init_chrome.
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

BrowserView::BrowserView(Browser* browser)
    : browser_(browser),
      map_pages_(std::make_unique<MapPagesChrome>(this)),
      processing_(std::make_unique<ProcessingChrome>(this)),
      inspect_(std::make_unique<InspectChrome>(this)),
      inspector_sync_(std::make_unique<InspectorSyncChrome>(this)),
      debug_console_(std::make_unique<DebugConsoleChrome>(this)),
      atmosphere_(std::make_unique<AtmosphereChrome>(this)),
      shell_layout_(std::make_unique<ShellLayoutChrome>(this)) {}

BrowserView::~BrowserView() {
  prepare_chrome_close();
}

void BrowserView::prepare_chrome_close() {
  remove_shell_wheel_forward();
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

namespace {
constexpr UINT_PTR kShellWheelSubclassId = 0x57484C45u;  // 'WHLE'
}  // namespace

void BrowserView::install_shell_wheel_forward() {
  HWND shell = widget_.hwnd();
  if (!shell || !IsWindow(shell) || shell_wheel_subclassed_) {
    return;
  }
  if (SetWindowSubclass(shell, shell_wheel_subclass_proc, kShellWheelSubclassId,
                        reinterpret_cast<DWORD_PTR>(this))) {
    shell_wheel_subclassed_ = true;
  }
}

void BrowserView::remove_shell_wheel_forward() {
  HWND shell = widget_.hwnd();
  if (shell_wheel_subclassed_ && shell && IsWindow(shell)) {
    RemoveWindowSubclass(shell, shell_wheel_subclass_proc,
                         kShellWheelSubclassId);
  }
  shell_wheel_subclassed_ = false;
}

LRESULT CALLBACK BrowserView::shell_wheel_subclass_proc(HWND hwnd, UINT msg,
                                                       WPARAM wparam,
                                                       LPARAM lparam,
                                                       UINT_PTR id,
                                                       DWORD_PTR data) {
  auto* self = reinterpret_cast<BrowserView*>(data);
  // Posted by deferred China seed when SMT_VIEWS_START_MAP_TAB is set — must
  // not nest select_map_tab inside the seed timer / switch_map_tab wait.
  constexpr UINT kReselectTab = WM_APP + 0x5354;  // 'ST'
  if (self && id == kShellWheelSubclassId && msg == kReselectTab) {
    const int idx = static_cast<int>(wparam);
    if (idx >= 0 && idx <= 2) {
      self->select_map_tab(idx);
      LOGGING(LOG_INFO, "startup: posted reselect map tab=%d after China seed",
              idx);
    }
    return 0;
  }
  if (self && id == kShellWheelSubclassId &&
      (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)) {
    // FlyCube present uses SW_SHOWNOACTIVATE; focus stays on chrome so wheel
    // arrives here. Forward when the cursor is over Map / Data / 3D input.
    const POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    for (ui::views::MapViewport* pane :
         {self->map_edit_, self->map_data_, self->map_scene_}) {
      if (!pane) {
        continue;
      }
      HWND map = pane->input_hwnd();
      if (!map || !IsWindow(map) || !IsWindowVisible(map)) {
        continue;
      }
      RECT rc = {};
      GetWindowRect(map, &rc);
      if (PtInRect(&rc, pt)) {
        SendMessageW(map, msg, wparam, lparam);
        return 0;
      }
    }
  }
  return DefSubclassProc(hwnd, msg, wparam, lparam);
}

bool BrowserView::init_chrome() {
  BASE_TRACE_EVENT("InitChrome.body", "startup");
  {
    BASE_TRACE_EVENT("Widget.init", "startup");
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
  }
  // Mid snapshots → *.partial-*.txt; final dump is after first show only.
  base::trace::dump_startup_profile_partial("post-widget");
  widget_.set_will_close([this]() {
    if (browser_) {
      browser_->prepare_close();
    }
  });
  widget_.set_on_shell_published(
      [this](const ui::views::Rect& dirty) { commit_widget_shell_to_maps(dirty); });
  install_shell_wheel_forward();

  {
    BASE_TRACE_EVENT("BuildContents", "startup");
    build_contents();
  }
  base::trace::dump_startup_profile_partial("post-build");
  {
    BASE_TRACE_EVENT("SeedDocument", "startup");
    // Showcase / harness set SMT_SKIP_AMBOX_CATALOG before Browser::init.
    // Skip china OGR bootstrap: GDAL open of china_city.* can hang so long
    // that plugin-showcase never reaches run_world3d_scene3d (no marks).
    // Scene3D True Earth frames via product orbit defaults instead.
    // Product path sets defer_china_seed(): demo seed here, China/DEM after
    // first show (Browser::show timer) so WaitFirstMapPresent is not ~10s+.
    const bool skip_china_seed = []() {
      const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
      return skip && skip[0] != '\0' && skip[0] != '0';
    }();
    const bool defer_china = browser_->defer_china_seed();
    if (skip_china_seed || defer_china) {
      if (skip_china_seed) {
        std::fprintf(stderr,
                     "startup: SeedDocument demo-only (SMT_SKIP_AMBOX_CATALOG)\n");
      } else {
        std::fprintf(stderr,
                     "startup: SeedDocument demo-only (defer_china_seed)\n");
      }
      browser_->document()->seed_default(/*allow_china_bootstrap=*/false);
    } else {
      {
        BASE_TRACE_EVENT("SeedDocument.Default", "startup");
        browser_->document()->seed_default(/*allow_china_bootstrap=*/true);
      }
      // Zero-argv / harness sync path: seed_default only opens china when
      // try_bootstrap_china_plp finds a file. Mirror showcase via the shared
      // sample opener so fit_map_extent can apply China carto (has_china_extent).
      if (browser_->document() && !browser_->document()->has_china_extent()) {
        BASE_TRACE_EVENT("try_open_china", "startup");
        (void)detail::try_open_china_sample(*browser_,
                                            /*write_stub_if_missing=*/false);
      }
    }
  }
  {
    BASE_TRACE_EVENT("BindPresenters", "startup");
    browser_->map2d()->bind(browser_->document(), browser_->view_frame());
    browser_->scene3d()->bind_orbit(browser_->orbit_frame());
    browser_->scene3d()->bind_label_frame(browser_->view_frame());
    browser_->scene3d()->bind_map(browser_->document());
    browser_->pull_orbit_extent();
    // Fit world extent BEFORE FlyCube attach so the first display-thread
    // present_gpu uses a real camera (not a degenerate default extent).
    // Showcase skip-china seed: fit_map_extent AVd on demo-only document /
    // skewed ui_ hwnd during early init (cdb world3d-early). Scene3D framing
    // is applied later by apply_china_scene3d_product_defaults.
    const bool skip_fit = []() {
      const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
      return skip && skip[0] != '\0' && skip[0] != '0';
    }();
    if (!skip_fit) {
      browser_->fit_map_extent();
    }
    browser_->push_shared_extent();
    wire_map_scene();
  }
  // Snapshot before FlyCube attach — often the slowest / hangiest startup step.
  base::trace::dump_startup_profile_partial("pre-attach");
  {
    BASE_TRACE_EVENT("AttachViewports", "startup");
    attach_viewports();
  }
  browser_->scene3d()->bind_contents(
      browser_->map_session(), map_scene_ ? map_scene_->view_id() : 0);
  browser_->pull_orbit_extent();
  if (browser_->map_session()) {
    browser_->map_session()->SetObserver(browser_);
  }
  {
    BASE_TRACE_EVENT("WireChrome", "startup");
    attach_hwnd_gestures();
    wire_catalog();
    wire_edit_feedback();
    // Re-fit after HWND sizes settle (layout may change client rect post-attach).
    // Same showcase skip as BindPresenters — demo-only seed AVs in fit_map_extent.
    if (const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
        !(skip && skip[0] != '\0' && skip[0] != '0')) {
      browser_->fit_map_extent();
    }
    browser_->push_shared_extent();
    // China 3D atmosphere (same defaults as --atmosphere-showcase=full) is
    // seeded on first switch to the 3D tab — see apply_china_scene3d_* in
    // switch_map_tab — so init_chrome does not pay DEM/atmosphere cost before
    // the Map pane is interactive.
    sync_inspectors_from_scene();
    sync_status();
  }
  return true;
}

void BrowserView::show_chrome() {
  BASE_TRACE_EVENT("ShowChrome", "startup");
  {
    BASE_TRACE_EVENT("ShowWindow", "startup");
    widget_.show();
  }
  // ShowWindow may present an empty compositor front (async raster). Re-layout
  // and schedule shell paint only — do not call invalidate_map_overlays() here:
  // that syncs paint_map_content while ContentMapView / Map2dPresenter are
  // still settling and has AVd in Map2dSoftwarePainter (STL orphan) under
  // --self-test. Kick the active map HWND asynchronously (InvalidateRect,
  // no UpdateWindow).
  widget_.layout_contents();
  // Catalog|Map splitter can lock a both-flex seed before preferred widths
  // settle (grey slab + squeezed map). Re-assert Catalog 288 DIP and reseed.
  if (catalog_ && catalog_map_) {
    catalog_->set_preferred_size({288, 0});
    catalog_map_->reseed();
    widget_.layout_contents();
    LOGGING(LOG_INFO, "layout: catalog_map reseed catalog_w=%d map_tabs_x=%d",
            catalog_->bounds().width,
            map_tabs_ ? map_tabs_->bounds().x : -1);
  }
  widget_.schedule_paint();
  if (HWND shell = widget_.hwnd()) {
    if (IsWindow(shell)) {
      InvalidateRect(shell, nullptr, FALSE);
    }
  }
  if (ui::views::MapViewport* pane = active_map()) {
    pane->sync_native_bounds();
    // Init may have finished while the shell was still hidden; lift the DXGI
    // popup now that chrome is shown (inactive tabs stay hidden below).
    // Also bumps request_frame for the first china present.
    pane->set_flycube_present_visible(true);
    // Present may have been revealed after the first gesture attach (embed
    // only). Rebind to input_hwnd() so pan/pinch/right-click hit the DXGI
    // surface under plain (no-arg) launch.
    attach_hwnd_gestures();
    // Do NOT fit_map_extent / invalidate_frame_cache here: Display may hold
    // the map2d cache mutex on the first china present (~8s Debug). Fit's
    // overlay invalidate can also re-enter while this pump waits. Browser::show
    // fits after show_chrome returns.
    pane->invalidate_native();
    if (HWND map = pane->native_view()) {
      if (IsWindow(map)) {
        InvalidateRect(map, nullptr, FALSE);
      }
    }
    // Product path: do not block shell interactivity on a full first map
    // present (FlyCube token + carto layout often ~2s+ and nested HillshadeBake).
    // Invalidate above already schedules the first frame; China seed (when
    // deferred) refreshes after show. Opt-in sync wait for harness / agents
    // that need a deterministic first carto frame before continuing:
    //   SMT_SYNC_FIRST_MAP_PRESENT=1
    // Showcase skips Map Edit present attach (SMT_SKIP_AMBOX_CATALOG) — never
    // spin waiting for a frame that will never arrive.
    const bool skip_wait = []() {
      const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
      if (skip && skip[0] != '\0' && skip[0] != '0') {
        return true;
      }
      const char* sync = std::getenv("SMT_SYNC_FIRST_MAP_PRESENT");
      const bool want_sync = sync && sync[0] == '1' && sync[1] == '\0';
      return !want_sync;
    }() || pane->attach_mode() == ui::views::MapViewport::AttachMode::kNone;
    if (!skip_wait) {
      BASE_TRACE_EVENT("WaitFirstMapPresent", "startup");
      uint32_t want = pane->frame_request();
      HWND shell_hwnd = widget_.hwnd();
      const DWORD t0 = GetTickCount();
      const bool content_sot =
          pane->attach_mode() ==
          ui::views::MapViewport::AttachMode::kContentMapView;
      while (shell_hwnd && IsWindow(shell_hwnd) &&
             GetTickCount() - t0 < 15000u) {
        if (content_sot) {
          // GDI overlay / SharedSurface SoT: layout rebuild or content blit.
          if (pane->last_content_present_ok() ||
              (browser_->map2d() &&
               browser_->map2d()->layout_build_count() > 0)) {
            break;
          }
        } else {
          const bool token_ok = pane->last_gpu_present_ok() &&
                                pane->frame_presented() >= want;
          const bool drew_carto =
              browser_->map2d() && browser_->map2d()->last_gpu_present_drew() &&
              browser_->map2d()->layout_build_count() > 0;
          if (token_ok && drew_carto) {
            break;
          }
          if (token_ok && !drew_carto && browser_->map2d()) {
            // Skip (or empty layout) satisfied the token — force a real Pass
            // submit before leaving the pump.
            browser_->map2d()->note_surface_reset();
            pane->invalidate_native();
            want = pane->frame_request();
          }
        }
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
        Sleep(10);
      }
    }
  }
  if (map_data_ && map_data_ != active_map()) {
    map_data_->set_flycube_present_visible(false);
  }
  if (map_scene_ && map_scene_ != active_map()) {
    map_scene_->set_flycube_present_visible(false);
  }
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
  if (shell_layout_) {
    shell_layout_->build_contents();
  }
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
  if (!catalog_ || !browser_ || !browser_->document()) {
    return;
  }
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  for (const content::LayerDesc& d : browser_->document()->layer_descs()) {
    ui::views::LayerTree::LayerDesc row;
    row.id = d.id;
    // china_city PLPT stems are short (area/line/point/text); show product
    // labels so the Layers panel stays legible on dark chrome.
    if (d.name == "area") {
      row.name = "Land";
    } else if (d.name == "line") {
      row.name = "Lines";
    } else if (d.name == "point") {
      row.name = "Points";
    } else if (d.name == "text") {
      row.name = "Labels";
    } else {
      row.name = d.name;
    }
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
  // Soft-skip catalog walk when parallel rebuilds leave CommandCatalog maps
  // unreadable (AV in tool::CommandCatalog::for_each). FPS bench and map2d /
  // plugin showcases set these env gates from BrowserMain.
  if (const char* bench = std::getenv("SMT_MAP2D_FPS_BENCH_MS")) {
    if (bench[0] != '\0' && std::atoi(bench) > 0) {
      return;
    }
  }
  // Match wire_report_panel / wire_edit_feedback: any non-empty non-"0" skip.
  if (const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
      skip && skip[0] != '\0' && skip[0] != '0') {
    return;
  }
  std::vector<tool::CommandCatalog*> catalogs;
  if (browser_->edit_host() && browser_->edit_host()->workspace()) {
    catalogs.push_back(&browser_->edit_host()->workspace()->catalog());
  }
  // Skip PluginShell::commands()/registry on first BuildContents: parallel
  // Debug links have AVd catalog_.get() on freefill (0xCD) while plugins()
  // still looks live. Workspace catalog is enough until on_plugins refreshes.
  std::vector<ui::views::AmboxView::Group> plugin_groups;
  if (ambox_include_plugins_ && browser_->plugins()) {
    PluginShell* shell = browser_->plugins();
    const auto shell_addr = reinterpret_cast<uintptr_t>(shell);
    // MSVC Debug freefill / freed-heap markers (populate_ambox AV dumps).
    const auto lo24 = shell_addr & 0xffffff00ull;
    const bool poison = shell_addr < 0x10000u || lo24 == 0xcdcdcd00ull ||
                        lo24 == 0xdddddd00ull || lo24 == 0xcccccc00ull ||
                        lo24 == 0xfeeefeeeull || lo24 == 0xababab00ull;
    if (!poison) {
      if (tool::CommandCatalog* plugin_catalog = shell->commands()) {
        catalogs.push_back(plugin_catalog);
      }
      if (shell->registry() && shell->host()) {
        plugin_groups =
            enabled_plugin_groups(shell->registry(), shell->host());
      }
    }
  }
  // Horizontal map tool bar + vertical right-dock AMBox share the same groups.
  ambox_->populate_from_commands(catalogs, plugin_groups);
  if (side_ambox_) {
    side_ambox_->populate_from_commands(catalogs, std::move(plugin_groups));
  }
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
    (void)browser_->plugins()->ensure_builtins();
    // Re-bind after builtins so report callbacks see a live PluginHost.
    ensure_inspector_tab(report_tab_);
    attach_report_plugin_bridge();
    browser_->plugins()->show_manager(widget_.hwnd());
    ambox_include_plugins_ = true;
    populate_ambox();
    ambox_include_plugins_ = false;
  }
}

void BrowserView::on_processing() {
  // Prefer Analysis; fall back to Processing — both may be lazy.
  const int prefer =
      spatial_analysis_tab_ >= 0 ? spatial_analysis_tab_ : processing_tab_;
  ensure_inspector_tab(prefer);
  ensure_inspector_tab(processing_tab_);
  show_inspector_tab_index(prefer);
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
  ensure_inspector_tab(index);
  if (inspector_tabs_ && index >= 0) {
    inspector_tabs_->set_active(index);
  }
}

void BrowserView::ensure_inspector_tab(int index) {
  if (!inspector_tabs_ || index < 0) {
    return;
  }
  if (index == measure_tab_ && !measure_panel_) {
    auto panel = std::make_unique<ui::views::MeasurePanel>();
    measure_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_measure_panel();
  } else if (index == selection_tab_ && !selection_panel_) {
    auto panel = std::make_unique<ui::views::SelectionPanel>();
    selection_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_selection_panel();
  } else if (index == layer_props_tab_ && !layer_properties_panel_) {
    auto panel = std::make_unique<ui::views::LayerPropertiesPanel>();
    layer_properties_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_layer_properties_panel();
  } else if (index == legend_tab_ && !legend_panel_) {
    auto panel = std::make_unique<ui::views::LegendPanel>();
    legend_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_legend_panel();
  } else if (index == spatial_analysis_tab_ && !spatial_analysis_panel_) {
    auto panel = std::make_unique<ui::views::SpatialAnalysisPanel>();
    spatial_analysis_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_spatial_analysis_panel();
  } else if (index == processing_tab_ && !processing_panel_) {
    auto panel = std::make_unique<ui::views::ProcessingPanel>();
    processing_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_processing_panel();
  } else if (index == playback_tab_ && !result_playback_panel_) {
    auto panel = std::make_unique<ui::views::ResultPlaybackPanel>();
    result_playback_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_result_playback_panel();
  } else if (index == report_tab_ && !report_panel_) {
    auto panel = std::make_unique<ReportPanel>();
    report_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    // P1-3: WebView2 ReportBrowser is created inside wire_report_panel.
    wire_report_panel();
  } else if (index == atmosphere_tab_ && !atmosphere_panel_) {
    auto panel = std::make_unique<ui::views::AtmospherePanel>();
    atmosphere_panel_ = panel.get();
    inspector_tabs_->replace_page(index, std::move(panel));
    wire_atmosphere_panel();
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

// Thin forwards to chrome composers (deep split of former multi-TU
// BrowserView method bodies).

void BrowserView::attach_viewports() {
  map_pages_->attach_viewports();
}

void BrowserView::wire_map_scene() {
  map_pages_->wire_map_scene();
}

void BrowserView::commit_widget_shell_to_maps() {
  map_pages_->commit_widget_shell_to_maps();
}

void BrowserView::commit_widget_shell_to_maps(const ui::views::Rect& dirty) {
  map_pages_->commit_widget_shell_to_maps(dirty);
}

void BrowserView::sync_flash_timer() {
  map_pages_->sync_flash_timer();
}

void BrowserView::wire_tool_seams() {
  map_pages_->wire_tool_seams();
}

void BrowserView::for_each_map_viewport(const std::function<void(ui::views::MapViewport*)>& fn) const {
  map_pages_->for_each_map_viewport(fn);
}

void BrowserView::invalidate_map_overlays() {
  map_pages_->invalidate_map_overlays();
}

void BrowserView::attach_hwnd_gestures() {
  map_pages_->attach_hwnd_gestures();
}

void BrowserView::configure_gestures(MapHwndGestures* gestures) {
  map_pages_->configure_gestures(gestures);
}

void BrowserView::active_view_size(int* w, int* h) const {
  map_pages_->active_view_size(w, h);
}

void BrowserView::switch_map_tab(int i) {
  map_pages_->switch_map_tab(i);
}

ui::views::MapViewport* BrowserView::active_map() const {
  return map_pages_->active_map();
}

content::ViewHost* BrowserView::active_view_host() const {
  return map_pages_->active_view_host();
}

void BrowserView::wire_processing_panel() {
  processing_->wire_processing_panel();
}

void BrowserView::wire_result_playback_panel() {
  processing_->wire_result_playback_panel();
}

void BrowserView::wire_report_panel() {
  processing_->wire_report_panel();
}

void BrowserView::attach_report_plugin_bridge() {
  processing_->attach_report_plugin_bridge();
}

void BrowserView::sync_result_playback_timer() {
  processing_->sync_result_playback_timer();
}

void BrowserView::wire_spatial_analysis_panel() {
  processing_->wire_spatial_analysis_panel();
}

void BrowserView::run_processing_operator(const std::string& processing_id) {
  processing_->run_processing_operator(processing_id);
}

void BrowserView::run_processing_operator(const std::string& processing_id, const std::string& distance) {
  processing_->run_processing_operator(processing_id, distance);
}

void BrowserView::bind_gis_python_bridge() {
  processing_->bind_gis_python_bridge();
}

void BrowserView::wire_measure_panel() {
  inspect_->wire_measure_panel();
}

void BrowserView::wire_selection_panel() {
  inspect_->wire_selection_panel();
}

void BrowserView::wire_layer_properties_panel() {
  inspect_->wire_layer_properties_panel();
}

void BrowserView::wire_legend_panel() {
  inspect_->wire_legend_panel();
}

bool BrowserView::try_consume_measure_draft(const tool::Draft& draft) {
  return inspect_->try_consume_measure_draft(draft);
}

void BrowserView::sync_selection_panel_from_scene() {
  inspect_->sync_selection_panel_from_scene();
}

void BrowserView::sync_legend_panel_from_scene() {
  inspect_->sync_legend_panel_from_scene();
}

void BrowserView::sync_layer_properties_from_scene() {
  inspect_->sync_layer_properties_from_scene();
}

bool BrowserView::invert_selection() {
  return inspect_->invert_selection();
}

bool BrowserView::export_selection_geojson(std::string* out_path) {
  return inspect_->export_selection_geojson(out_path);
}

void BrowserView::sync_inspectors_from_scene() {
  inspector_sync_->sync_inspectors_from_scene();
}

void BrowserView::sync_result_playback_from_session() {
  inspector_sync_->sync_result_playback_from_session();
}

void BrowserView::wire_edit_feedback() {
  inspector_sync_->wire_edit_feedback();
}

void BrowserView::bind_debug_agent_host() {
  debug_console_->bind_debug_agent_host();
}

void BrowserView::wire_debug_console() {
  debug_console_->wire_debug_console();
}

void BrowserView::toggle_debug_console() {
  debug_console_->toggle_debug_console();
}

void BrowserView::wire_atmosphere_panel() {
  atmosphere_->wire_atmosphere_panel();
}

}  // namespace app
