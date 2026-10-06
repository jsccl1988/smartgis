// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/bind_horizon.h"

#include <string>
#include <vector>

#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/bind/slots.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/paths.h"
#include "app/views/il.runtime/backend/pump.h"
#include "app/views/il.runtime/backend/stress.h"
#include "app/views/il.runtime/backend/surface.h"
#include "app/views/il.runtime/backend/horizon_document.h"
#include "app/views/il.runtime/backend/still.h"
#include "app/views/il.runtime/backend/ui.h"
#include "app/views/util/charset.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

void bind_chrome(Browser& browser,
                 const wchar_t* leaf,
                 content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_pump> =
                   [](int ms) {
                     pump_messages(static_cast<DWORD>(ms > 0 ? ms : 0));
                   },
               base::tag_resolver<slot_mark> =
                   [leaf](const std::string& token) {
                     write_mark(leaf, token.c_str(), false);
                   },
               base::tag_resolver<slot_select_map_tab> =
                   [b](int index) { b->select_map_tab(index); },
               base::tag_resolver<slot_catalog_tab> =
                   [b](int index) {
                     if (ui::views::CatalogView* cat = b->catalog_view()) {
                       if (ui::views::TabStrip* tabs = cat->source_tabs()) {
                         tabs->set_active(index);
                       }
                     }
                   },
               base::tag_resolver<slot_inspector_tab> =
                   [b](int index) {
                     if (b->ui()) {
                       b->ui()->activate_inspector_tab(index);
                     }
                   },
               base::tag_resolver<slot_clear_marks> =
                   [leaf]() { clear_mark(leaf); },
               base::tag_resolver<slot_suppress_dialogs> =
                   [](bool on) {
                     ui::views::Dialog::set_dialog_modals_suppressed_for_test(
                         on);
                     return true;
                   },
           });
}

void bind_surface(Browser& browser, content::CapabilityHost* out) {
  ShellSurface surface(browser);
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_shell_hwnd> =
                   [surface]() -> void* { return surface.hwnd_ptr(); },
               base::tag_resolver<slot_require_hwnd> =
                   [surface]() { return surface.alive(); },
               base::tag_resolver<slot_post_click> =
                   [surface](int x, int y, int button, int clicks) {
                     return surface.click(x, y, button, clicks);
                   },
               base::tag_resolver<slot_post_drag> =
                   [surface](int x0, int y0, int x1, int y1) {
                     return surface.drag(x0, y0, x1, y1);
                   },
               base::tag_resolver<slot_post_wheel> =
                   [surface](int x, int y, int delta) {
                     return surface.wheel(x, y, delta);
                   },
               base::tag_resolver<slot_post_path> =
                   [surface](const std::vector<int>& xs,
                             const std::vector<int>& ys) {
                     return surface.path(xs, ys);
                   },
               base::tag_resolver<slot_post_key> =
                   [surface](unsigned vk, bool down) {
                     return surface.key(vk, down);
                   },
               base::tag_resolver<slot_vk_from_name> =
                   [](const std::string& name) { return vk_from_name(name); },
               base::tag_resolver<slot_window> =
                   [surface](const std::string& action, int w, int h) {
                     return surface.action(action, w, h);
                   },
               base::tag_resolver<slot_key> =
                   [surface](unsigned vk) { return surface.tap_key(vk); },
           });
}

void bind_document_slots(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_dispatch_edit_input> =
                   [b](const content::InputEvent& e) {
                     return dispatch_edit_input(*b, e);
                   },
               base::tag_resolver<slot_open_map> =
                   [b](const std::string& path_utf8) {
                     return open_map_document(*b, path_utf8);
                   },
               base::tag_resolver<slot_detach_maps> =
                   [b]() { detach_maps(*b); },
               base::tag_resolver<slot_stop_map_present_timers> =
                   [b]() { stop_map_present_timers(*b); },
               base::tag_resolver<slot_resume_map_present_timers> =
                   [b]() { resume_map_present_timers(*b); },
               base::tag_resolver<slot_run_tool> =
                   [b](const std::string& command_id) {
                     return b->run_tool_command(command_id);
                   },
               base::tag_resolver<slot_doc_clear> =
                   [b]() { return clear_map_document(*b); },
               base::tag_resolver<slot_fit_extent> =
                   [b]() { return fit_map_document(*b); },
               base::tag_resolver<slot_apply_style_file> =
                   [b](const std::string& path_utf8) {
                     return apply_style_file(*b, path_utf8);
                   },
               base::tag_resolver<slot_invalidate_map2d> =
                   [b]() { return invalidate_map2d_frame(*b); },
           });
}

void bind_browse_slots(Browser& browser,
                       const wchar_t* leaf,
                       content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_browse_stress> =
                   [b, leaf](int count) {
                     return browse_stress(*b, leaf, count);
                   },
               base::tag_resolver<slot_capture_browse_still> =
                   [b, leaf](const std::string& face) {
                     if (face == "scene3d") {
                       capture_browse_scene3d(*b);
                       return true;
                     }
                     if (face == "map2d") {
                       (void)capture_browse_map2d(*b, leaf);
                       return true;
                     }
                     return false;
                   },
           });
}

void bind_ui_slots(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_apply_ui_theme> =
                   []() {
                     apply_ui_harness_theme();
                     return true;
                   },
               base::tag_resolver<slot_apply_scenario_panels> =
                   [b](const std::string& mode) {
                     apply_ui_scenario_panels(*b,
                                              ui_showcase_mode_from_name(mode));
                     return true;
                   },
               base::tag_resolver<slot_layout_gate> =
                   [b](const std::string& mode) {
                     return run_ui_layout_gate(
                         *b, ui_showcase_mode_from_name(mode));
                   },
               base::tag_resolver<slot_ui_present_capture> =
                   [b](const std::string& mode) {
                     return run_ui_present_capture(
                         *b, ui_showcase_mode_from_name(mode));
                   },
               base::tag_resolver<slot_fps_bench> =
                   [b]() {
                     run_horizon_map2d_fps_bench(*b);
                     return true;
                   },
               base::tag_resolver<slot_capture_shell_bmp> =
                   [b](const std::string& leaf_utf8) {
                     if (leaf_utf8.empty() || !b->hwnd()) {
                       return false;
                     }
                     const std::wstring wleaf = utf8_to_wide(leaf_utf8);
                     if (wleaf.empty()) {
                       return false;
                     }
                     HWND map_hwnd = nullptr;
                     if (ui::views::DrawHost* pane = b->draw_host()) {
                       map_hwnd = pane->native_view();
                     }
                     return capture_ui_shell_bmp(b->hwnd(), wleaf.c_str(),
                                                 map_hwnd, nullptr);
                   },
           });
}

}  // namespace

void bind_horizon(Browser& browser,
                  content::CapabilityHost* out,
                  const wchar_t* mark_leaf) {
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : kUiShowcaseMarkLeaf;
  bind_chrome(browser, leaf, out);
  bind_surface(browser, out);
  bind_paths(out);
  bind_document_slots(browser, out);
  bind_browse_slots(browser, leaf, out);
  bind_ui_slots(browser, out);
}

}  // namespace detail
}  // namespace app
