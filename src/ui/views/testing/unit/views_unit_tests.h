// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_TESTS_H_
#define UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_TESTS_H_

// Declarations for domain-split views_unittests console self-tests.

void test_utf8_and_theme();
void test_painter_registry_and_delegate();
void test_shell_canvas_preference();
void test_skia_canvas_api();
void test_kernel_visible_enabled_focus_hover();
void test_tab_focus_traversal();
void test_box_layout_skips_hidden();
void test_button_send_mouse();
void test_label_button_preferred_from_measure();
void test_textfield_set_text_char_backspace();
void test_checkbox_toggle();
void test_slider_and_atmosphere_panel();
void test_radio_exclusive_group();
void test_combobox_select();
void test_tab_strip_switch_page();
void test_table_and_attribute_selection();
void test_layer_tree();
void test_catalog_view();
void test_feature_info_and_status_bar();
void test_splitter_layout();
void test_splitter_host_resize_grows_flex_pane();
void test_splitter_drag_keeps_capture();
void test_layer_tree_add_while_hidden();
void test_ambox_in_view_tree();
void test_ambox_populate_from_commands();
void test_ambox_plugin_groups();
void test_tree_view_add_select_check();
void test_tree_view_dpi_row_height();
void test_scroll_view_wheel();
void test_menu_bar_click();
void test_menu_bar_add_menu();
void test_ambox_skips_view_navigation();
void test_layout_invariants_smoke();
void test_sibling_overlap_detection();
void test_gantt_lane_geom_spaced();
void test_tab_strip_catalog_labels_have_cells();
void test_tab_strip_packed_not_equal_width();
void test_ambox_buttons_not_collapsed();
void test_forensics_dump_writes_manifest();
void test_tab_strip_page_bounds_align();
void test_box_layout_insets_and_spacing();
void test_box_layout_flex_keeps_preferred();
void test_box_layout_flex_share_no_stack();
void test_box_layout_overflow_fits_host();
void test_box_layout_preferred_size_from_children();
void test_dialog_close_noop();
void test_dialog_host_geometry();
void test_layout_center_helper();
void test_widget_hwnd_and_map_viewport();
void test_custom_frame_hides_os_caption();
void test_touch_multitouch_midpoint();
void test_dpi_scale_math();
void test_device_scale_recomputes_preferred();
void test_combobox_dpi_row_geometry();
void test_status_bar_dpi_height();
void test_paint_fingerprint_locked_scene();
void test_ambox_scroll_content_taller_than_pane();
void test_dialog_host_clamps_to_work_area();
void test_hover_paint_skips_unrelated_views();
void test_paint_commit_snapshot_isolation();
void test_paint_commit_dirty_culls_commands();
void test_shell_compositor_async_publish_wake();
void test_shell_compositor_present_fills_when_buffer_lags();
void test_shell_compositor_present_no_flash_when_front_covers();
void test_set_layers_layouts_once();
void test_scroll_skips_layout_when_preferred_unchanged();
void test_table_paints_viewport_rows_only();
void test_table_row_cache_hit_on_rerecord();
void test_set_text_caches_measure();
void test_vblank_clock_wait_returns();

#endif  // UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_TESTS_H_
