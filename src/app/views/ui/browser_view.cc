// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/browser_view.h"

#include "app/views/util/charset.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/browser.h"
#include "app/views/ui/horizon/ambox_composer.h"
#include "app/views/ui/horizon/catalog_composer.h"
#include "app/views/ui/horizon/inspector_host_composer.h"
#include "app/views/ui/horizon/menu_composer.h"
#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/panels/debug_console_composer.h"
#include "app/views/ui/panels/inspect_composer.h"
#include "app/views/ui/panels/inspector_sync_composer.h"
#include "app/views/ui/panels/processing_composer.h"
#include "app/views/ui/shell/shell_layout_composer.h"
#include "app/views/ui/shell/shell_lifecycle_composer.h"
#include "tool/draft/draft.h"
#include "tool/nav/camera_nav.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {

std::unique_ptr<BrowserUiDelegate> create_browser_ui(Browser* browser) {
  return std::make_unique<BrowserView>(browser);
}

Browser* BrowserView::browser() const {
  return browser_;
}

BrowserView::BrowserView(Browser* browser)
    : browser_(browser),
      map_pages_(std::make_unique<MapPagesComposer>(this)),
      processing_(std::make_unique<ProcessingComposer>(this)),
      inspect_(std::make_unique<InspectComposer>(this)),
      inspector_sync_(std::make_unique<InspectorSyncComposer>(this)),
      debug_console_(std::make_unique<DebugConsoleComposer>(this)),
      shell_layout_(std::make_unique<ShellLayoutComposer>(this)),
      shell_lifecycle_(std::make_unique<ShellLifecycleComposer>(this)),
      catalog_composer_(std::make_unique<CatalogComposer>(this)),
      menu_composer_(std::make_unique<MenuComposer>(this)),
      ambox_composer_(std::make_unique<AmboxComposer>(this)),
      inspector_host_(std::make_unique<InspectorHostComposer>(this)) {}

BrowserView::~BrowserView() {
  prepare_shell_close();
}

bool BrowserView::init_shell() {
  return shell_lifecycle_->init_shell();
}

void BrowserView::show_shell() {
  shell_lifecycle_->show_shell();
}

void BrowserView::finish_deferred_shell_wiring() {
  shell_lifecycle_->finish_deferred_shell_wiring();
}

void BrowserView::prepare_shell_close() {
  shell_lifecycle_->prepare_shell_close();
}

void BrowserView::install_shell_wheel_forward() {
  shell_lifecycle_->install_shell_wheel_forward();
}

void BrowserView::remove_shell_wheel_forward() {
  shell_lifecycle_->remove_shell_wheel_forward();
}

int BrowserView::run_shell_loop() {
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
  catalog_composer_->wire_catalog();
}

void BrowserView::sync_catalog_from_scene() {
  catalog_composer_->sync_catalog_from_scene();
}

bool BrowserView::scene3d_tab_active() const {
  return map_tabs_ && map_tabs_->active() == 1;
}

void BrowserView::on_map_right_click(HWND map_hwnd, int view_x, int view_y) {
  menu_composer_->on_map_right_click(map_hwnd, view_x, view_y);
}

void BrowserView::show_pending_map_context_menu() {
  menu_composer_->show_pending_map_context_menu();
}

void BrowserView::schedule_menu_rebuild() {
  menu_composer_->schedule_menu_rebuild();
}

void BrowserView::rebuild_menus() {
  menu_composer_->rebuild_menus();
}

void BrowserView::populate_ambox() {
  ambox_composer_->populate_ambox();
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
  ambox_composer_->on_plugins();
}

void BrowserView::on_processing() {
  inspector_host_->on_processing();
}

void BrowserView::ensure_processing_panel() {
  inspector_host_->ensure_processing_panel();
}

void BrowserView::activate_inspector_tab(int index) {
  inspector_host_->activate_inspector_tab(index);
}

void BrowserView::show_inspector_tab_index(int index) {
  inspector_host_->show_inspector_tab_index(index);
}

void BrowserView::ensure_inspector_tab(int index) {
  inspector_host_->ensure_inspector_tab(index);
}

void BrowserView::sync_status() {
  ui::views::DrawHost* pane = active_map();
  if (!status_bar_ || !pane) {
    return;
  }
  status_bar_->set_status(detail::wide_to_utf8(pane->status_text()));
  // Keep Map|3D underline aligned with the live GPU present face. A mouse
  // set_active or plugin present_dataset can leave chrome on Map while
  // Scene3d is showing (plain-launch visual_review #6).
  if (map_tabs_ && map_scene_ && map_scene_->present_hwnd() &&
      IsWindow(map_scene_->present_hwnd()) &&
      IsWindowVisible(map_scene_->present_hwnd()) &&
      map_tabs_->active() != 1) {
    map_tabs_->set_active(1);
    map_tabs_->schedule_paint();
    widget_.schedule_paint();
  }
}

void BrowserView::select_view_tab(int index) {
  switch_map_tab(index);
}

void BrowserView::show_feature_info_tab() {
  inspector_host_->show_feature_info_tab();
}

void BrowserView::schedule_overlay_full_redraw() {
  HWND h = nullptr;
  if (ui::views::DrawHost* pane = active_map()) {
    h = pane->native_view();
  }
  if (!h) {
    h = hwnd();
  }
  if (!h) {
    return;
  }
  constexpr UINT_PTR kId = 0x424C54u;
  SetPropW(h, L"BlitBrowser", reinterpret_cast<HANDLE>(this));
  KillTimer(h, kId);
  SetTimer(h, kId, static_cast<UINT>(tool::kBlitDebounceMs),
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"BlitBrowser"));
             if (self && self->browser_) {
               self->browser_->commit_blit_preview();
             } else {
               InvalidateRect(timer_hwnd, nullptr, FALSE);
             }
           });
}

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

void BrowserView::for_each_draw_host(
    const std::function<void(ui::views::DrawHost*)>& fn) const {
  map_pages_->for_each_draw_host(fn);
}

void BrowserView::invalidate_map_overlays() {
  map_pages_->invalidate_map_overlays();
}

void BrowserView::invalidate_native_map() {
  if (map_edit_) {
    map_edit_->invalidate_native();
  }
}

void BrowserView::invalidate_native_data() {
  if (map_data_) {
    map_data_->invalidate_native();
  }
}

void BrowserView::invalidate_native_scene() {
  if (map_scene_) {
    map_scene_->invalidate_native();
  }
}

void BrowserView::pause_all_presents() {
  for_each_draw_host([](ui::views::DrawHost* pane) {
    if (pane) {
      pane->pause_present();
    }
  });
}

void BrowserView::resume_all_presents() {
  ui::views::DrawHost* active = active_map();
  for_each_draw_host([active](ui::views::DrawHost* pane) {
    if (!pane) {
      return;
    }
    pane->set_gpu_present_visible(pane == active);
    if (pane->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      return;
    }
    pane->resume_present_timer();
    pane->invalidate_native();
  });
}

void BrowserView::reattach_scene_draw_host() {
  if (!map_scene_) {
    return;
  }
  map_scene_->detach();
  (void)map_scene_->attach();
}

HWND BrowserView::scene_native_hwnd() const {
  return map_scene_ ? map_scene_->native_view() : nullptr;
}

uint32_t BrowserView::scene_view_id() const {
  return map_scene_ ? map_scene_->view_id() : 0;
}

void BrowserView::attach_hwnd_gestures() {
  map_pages_->attach_hwnd_gestures();
}

void BrowserView::configure_gestures(content::GisHwndGestures* gestures) {
  map_pages_->configure_gestures(gestures);
}

void BrowserView::active_view_size(int* w, int* h) const {
  map_pages_->active_view_size(w, h);
}

void BrowserView::switch_map_tab(int i) {
  map_pages_->switch_map_tab(i);
}

ui::views::DrawHost* BrowserView::active_map() const {
  return map_pages_->active_map();
}

content::ToolSession* BrowserView::active_tool_session() const {
  return map_pages_->active_tool_session();
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

void BrowserView::run_processing_operator(const std::string& processing_id,
                                          const std::string& distance) {
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

void BrowserView::attach_plugin_shell_ui() {
  inspector_host_->attach_plugin_shell_ui();
}

}  // namespace app
