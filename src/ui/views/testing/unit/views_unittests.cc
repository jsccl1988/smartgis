// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Console self-test for the public ui::views kernel and primitives.
// Domain bodies live in sibling *_unittests.cc under testing/unit/.
// Run: out\Debug\views_unittests.exe [--self-test]

#include <cstdio>

#include "ui/views/testing/unit/views_unit_helpers.h"
#include "ui/views/testing/unit/views_unit_tests.h"

int main() {
  test_utf8_and_theme();
  test_painter_registry_and_delegate();
  test_shell_canvas_preference();
  test_skia_canvas_api();
  test_kernel_visible_enabled_focus_hover();
  test_tab_focus_traversal();
  test_box_layout_skips_hidden();
  test_button_send_mouse();
  test_label_button_preferred_from_measure();
  test_textfield_set_text_char_backspace();
  test_checkbox_toggle();
  test_slider_and_atmosphere_panel();
  test_radio_exclusive_group();
  test_combobox_select();
  test_tab_strip_switch_page();
  test_table_and_attribute_selection();
  test_layer_tree();
  test_catalog_view();
  test_feature_info_and_status_bar();
  test_splitter_layout();
  test_splitter_host_resize_grows_flex_pane();
  test_splitter_drag_keeps_capture();
  test_layer_tree_add_while_hidden();
  test_ambox_in_view_tree();
  test_ambox_populate_from_commands();
  test_ambox_plugin_groups();
  test_tree_view_add_select_check();
  test_tree_view_dpi_row_height();
  test_scroll_view_wheel();
  test_menu_bar_click();
  test_menu_bar_add_menu();
  test_ambox_skips_view_navigation();
  test_layout_invariants_smoke();
  test_sibling_overlap_detection();
  test_gantt_lane_geom_spaced();
  test_tab_strip_catalog_labels_have_cells();
  test_tab_strip_packed_not_equal_width();
  test_ambox_buttons_not_collapsed();
  test_forensics_dump_writes_manifest();
  test_tab_strip_page_bounds_align();
  test_box_layout_insets_and_spacing();
  test_box_layout_flex_keeps_preferred();
  test_box_layout_flex_share_no_stack();
  test_box_layout_overflow_fits_host();
  test_box_layout_preferred_size_from_children();
  test_dialog_close_noop();
  test_dialog_host_geometry();
  test_layout_center_helper();
  test_widget_hwnd_and_map_viewport();
  test_custom_frame_hides_os_caption();
  test_touch_multitouch_midpoint();
  test_dpi_scale_math();
  test_device_scale_recomputes_preferred();
  test_combobox_dpi_row_geometry();
  test_status_bar_dpi_height();
  test_paint_fingerprint_locked_scene();
  test_ambox_scroll_content_taller_than_pane();
  test_dialog_host_clamps_to_work_area();
  test_hover_paint_skips_unrelated_views();
  test_paint_commit_snapshot_isolation();
  test_paint_commit_dirty_culls_commands();
  test_shell_compositor_async_publish_wake();
  test_shell_compositor_present_fills_when_buffer_lags();
  test_shell_compositor_present_no_flash_when_front_covers();
  test_set_layers_layouts_once();
  test_scroll_skips_layout_when_preferred_unchanged();
  test_table_paints_viewport_rows_only();
  test_table_row_cache_hit_on_rerecord();
  test_set_text_caches_measure();
  test_vblank_clock_wait_returns();

  if (g_fails) {
    std::fprintf(stderr, "views_unittests: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_unittests: ok\n");
  return 0;
}
