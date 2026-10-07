// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_BROWSER_VIEW_H_
#define APP_VIEWS_UI_BROWSER_VIEW_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/ui_delegate.h"
#include "ui/views/kernel/widget/widget.h"

namespace content {
class ToolSession;
}  // namespace content

namespace ui {
namespace views {
class AmboxView;
class AtmospherePanel;
class DiagnosticToolsPanel;
class AttributeTable;
class CatalogView;
class FeatureInfo;
class LayerPropertiesPanel;
class LegendPanel;
class DrawHost;
class MeasurePanel;
class MenuBar;
class ProcessingPanel;
class ResultPlaybackPanel;
class SelectionPanel;
class SpatialAnalysisPanel;
class Splitter;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace content {
class GisHwndGestures;
}  // namespace content

namespace app {

class AmboxComposer;
class Browser;
class CatalogComposer;
class DebugConsoleComposer;
class InspectComposer;
class InspectorHostComposer;
class InspectorSyncComposer;
class MapPagesComposer;
class MenuComposer;
class ProcessingComposer;
class ReportPanel;
class ShellLayoutComposer;
class ShellLifecycleComposer;

// Views horizon for SmartGisViews: MenuBar, splitters, TabStrip, map panes.
// Session / present ownership lives on Browser; this type holds Browser* and
// implements BrowserUiDelegate for status/tab UI push.
// Wire / lifecycle / chrome live in shell/ + horizon/ + pages/ + panels/
// *Composer helpers (friends) — see living shell §shell/ui composers.
class BrowserView : public BrowserUiDelegate {
  friend class AmboxComposer;
  friend class CatalogComposer;
  friend class DebugConsoleComposer;
  friend class InspectComposer;
  friend class InspectorHostComposer;
  friend class InspectorSyncComposer;
  friend class MapPagesComposer;
  friend class MenuComposer;
  friend class ProcessingComposer;
  friend class ShellLayoutComposer;
  friend class ShellLifecycleComposer;

 public:
  explicit BrowserView(Browser* browser);
  ~BrowserView() override;

  BrowserView(const BrowserView&) = delete;
  BrowserView& operator=(const BrowserView&) = delete;

  // Out-of-line: stale shell_ui .obj must not inline browser_ offsetof.
  Browser* browser() const;

  // BrowserUiDelegate
  bool init_shell() override;
  void show_shell() override;
  void finish_deferred_shell_wiring() override;
  int run_shell_loop() override;
  void prepare_shell_close() override;

  HWND hwnd() const override;
  ui::views::View* contents_view() const override;
  ui::views::CatalogView* catalog_view() const override { return catalog_; }
  ui::views::AmboxView* ambox_view() const override { return ambox_; }
  ui::views::StatusBar* status_bar() const override { return status_bar_; }
  ui::views::FeatureInfo* feature_info() const override {
    return feature_info_;
  }
  ui::views::AttributeTable* attribute_table() const override {
    return attribute_table_;
  }
  ui::views::ProcessingPanel* processing_panel() const override {
    return processing_panel_;
  }
  void ensure_processing_panel() override;
  ui::views::AtmospherePanel* atmosphere_panel() const override {
    return atmosphere_panel_;
  }
  ui::views::TabStrip* inspector_tabs() const override {
    return inspector_tabs_;
  }
  ui::views::DrawHost* draw_host() const override { return map_edit_; }
  ui::views::DrawHost* data_draw_host() const override {
    return map_data_;
  }
  ui::views::DrawHost* scene_draw_host() const override {
    return map_scene_;
  }

  ui::views::DrawHost* active_map() const override;
  content::ToolSession* active_tool_session() const override;
  void active_view_size(int* w, int* h) const override;
  bool scene3d_tab_active() const override;

  void set_status_message(const std::string& text) override;
  void invalidate_map_overlays() override;
  void sync_catalog_from_scene() override;
  void sync_inspectors_from_scene() override;
  void sync_flash_timer() override;
  void schedule_menu_rebuild() override;
  void schedule_overlay_full_redraw() override;
  void select_view_tab(int index) override;
  void activate_inspector_tab(int index) override;
  void show_feature_info_tab() override;
  void sync_status() override;
  void populate_ambox() override;
  void for_each_draw_host(
      const std::function<void(ui::views::DrawHost*)>& fn) const override;
  bool try_consume_measure_draft(const tool::Draft& draft) override;

  void invalidate_native_map() override;
  void invalidate_native_data() override;
  void invalidate_native_scene() override;
  void pause_all_presents() override;
  void resume_all_presents() override;
  void reattach_scene_draw_host() override;
  HWND scene_native_hwnd() const override;
  uint32_t scene_view_id() const override;

 private:
  void build_contents();
  void attach_viewports();
  void wire_catalog();
  void wire_edit_feedback();
  void wire_map_scene();
  void wire_processing_panel();
  void wire_result_playback_panel();
  void wire_report_panel();
  void attach_report_plugin_bridge();
  void sync_result_playback_timer();
  void wire_measure_panel();
  void wire_selection_panel();
  void wire_layer_properties_panel();
  void wire_legend_panel();
  void wire_spatial_analysis_panel();
  void wire_debug_console();
  void attach_plugin_shell_ui();
  void bind_debug_agent_host();
  void bind_gis_python_bridge();
  void toggle_debug_console();
  void show_inspector_tab_index(int index);
  void ensure_inspector_tab(int index);
  void commit_widget_shell_to_maps();
  void commit_widget_shell_to_maps(const ui::views::Rect& dirty);
  void attach_hwnd_gestures();
  void configure_gestures(content::GisHwndGestures* gestures);
  void install_shell_wheel_forward();
  void remove_shell_wheel_forward();
  void rebuild_menus();
  void on_map_right_click(HWND hwnd, int view_x, int view_y);
  void show_pending_map_context_menu();
  void on_exit();
  void on_plugins();
  void on_processing();
  void run_processing_operator(const std::string& processing_id);
  void run_processing_operator(const std::string& processing_id,
                               const std::string& distance);
  void switch_map_tab(int i);
  void wire_tool_seams();
  void sync_result_playback_from_session();
  void sync_selection_panel_from_scene();
  void sync_legend_panel_from_scene();
  void sync_layer_properties_from_scene();
  bool invert_selection();
  bool export_selection_geojson(std::string* out_path);

  Browser* browser_ = nullptr;

  ui::views::Widget widget_;
  ui::views::CatalogView* catalog_ = nullptr;
  ui::views::AmboxView* ambox_ = nullptr;
  ui::views::FeatureInfo* feature_info_ = nullptr;
  ui::views::AttributeTable* attribute_table_ = nullptr;
  ui::views::AtmospherePanel* atmosphere_panel_ = nullptr;
  ui::views::DiagnosticToolsPanel* diagnostic_tools_ = nullptr;
  ui::views::ProcessingPanel* processing_panel_ = nullptr;
  ui::views::MeasurePanel* measure_panel_ = nullptr;
  ui::views::SelectionPanel* selection_panel_ = nullptr;
  ui::views::LayerPropertiesPanel* layer_properties_panel_ = nullptr;
  ui::views::LegendPanel* legend_panel_ = nullptr;
  ui::views::SpatialAnalysisPanel* spatial_analysis_panel_ = nullptr;
  int measure_tab_ = -1;
  int selection_tab_ = -1;
  int layer_props_tab_ = -1;
  int legend_tab_ = -1;
  int spatial_analysis_tab_ = -1;
  int processing_tab_ = -1;
  int feature_info_tab_ = -1;
  int playback_tab_ = -1;
  int atmosphere_tab_ = -1;
  ui::views::TabStrip* inspector_tabs_ = nullptr;
  // Keep map_* contiguous and stable near the historical offset: inserting
  // playback/report fields above them skews stale map_pages.obj (parallel
  // ninja) so wire_map_scene calls set_overlay_paint on a garbage DrawHost*
  // → STATUS_HEAP_CORRUPTION / AV during Browser::init.
  ui::views::DrawHost* map_edit_ = nullptr;
  ui::views::DrawHost* map_data_ = nullptr;
  ui::views::DrawHost* map_scene_ = nullptr;
  ui::views::TabStrip* map_tabs_ = nullptr;
  ui::views::MenuBar* menu_bar_ = nullptr;
  ui::views::StatusBar* status_bar_ = nullptr;
  bool measure_armed_ = false;

  // Deferred map context menu (must not TrackPopupMenu on WM_RBUTTONUP stack).
  HWND pending_map_menu_hwnd_ = nullptr;
  int pending_map_menu_x_ = 0;
  int pending_map_menu_y_ = 0;

  // Newer horizon panels — append-only so older shell_ui TUs keep map_* offsets.
  ui::views::ResultPlaybackPanel* result_playback_panel_ = nullptr;
  ReportPanel* report_panel_ = nullptr;
  int report_tab_ = -1;

  // Last widget shell_generation() copied into a visible map overlay
  // (0 = never / cleared on map tab switch). U3 global gen-skip keys off this.
  std::uint64_t last_shell_overlay_gen_ = 0;

  // Per-pane crop cache (U3). Used to skip same-gen same-size memcpy; hidden
  // panes are not updated. Append-only — do not insert above map_*.
  struct LastShellOverlayCrop {
    ui::views::DrawHost* pane = nullptr;
    std::uint64_t gen = 0;
    int x0 = 0;
    int y0 = 0;
    int width = 0;
    int height = 0;
  };
  LastShellOverlayCrop last_shell_overlay_crops_[3]{};

  // Shell HWND subclass for wheel→map forward (append-only; do not insert
  // above map_* — parallel ninja stale .obj layout AV).
  bool shell_wheel_subclassed_ = false;
  // When true, populate_ambox walks PluginShell catalogs (on_plugins only).
  bool ambox_include_plugins_ = false;

  // Append-only: Catalog|Map splitter (reseed after show). Do not insert above
  // map_* — parallel ninja stale .obj layout AV.
  ui::views::Splitter* catalog_map_ = nullptr;

  // Append-only horizon composers (do not insert above map_*).
  std::unique_ptr<MapPagesComposer> map_pages_;
  std::unique_ptr<ProcessingComposer> processing_;
  std::unique_ptr<InspectComposer> inspect_;
  std::unique_ptr<InspectorSyncComposer> inspector_sync_;
  std::unique_ptr<DebugConsoleComposer> debug_console_;

  // Right-dock AMBox tab (vertical). Map tool bar is ambox_ (horizontal).
  // Append-only — do not insert above map_*.
  ui::views::AmboxView* side_ambox_ = nullptr;

  // Append-only: markup shell layout builder (do not insert above map_*).
  std::unique_ptr<ShellLayoutComposer> shell_layout_;

  // Append-only deep split (shell/ + horizon/) — do not insert above map_*.
  std::unique_ptr<ShellLifecycleComposer> shell_lifecycle_;
  std::unique_ptr<CatalogComposer> catalog_composer_;
  std::unique_ptr<MenuComposer> menu_composer_;
  std::unique_ptr<AmboxComposer> ambox_composer_;
  std::unique_ptr<InspectorHostComposer> inspector_host_;
};

}  // namespace app

#endif  // APP_VIEWS_UI_BROWSER_VIEW_H_
