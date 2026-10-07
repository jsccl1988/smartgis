// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/bind/bind_horizon.h"

#include <string>
#include <vector>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/bind/slots.h"
#include "app/views/il.runtime/backend/horizon/sema/debug_console.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/il.runtime/backend/horizon/sema/expect.h"
#include "app/views/il.runtime/backend/horizon/atom/surface.h"
#include "app/views/il.runtime/backend/horizon/sema/ui.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/primitives/collection/tab_strip.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

bool suppress_dialogs_slot(bool on) {
  ui::views::Dialog::set_dialog_modals_suppressed_for_test(on);
  return true;
}

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
               base::tag_resolver<slot_select_view_tab> =
                   [b](int index) { b->select_view_tab(index); },
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
           });
  // Prefer a function pointer: a bool(bool) lambda stored through
  // host_member/tagged_tuple has AVd on operator() under harness (slot
  // looks engaged; body never runs). Direct assign of the same pointer is OK.
  if (out) {
    out->horizon.suppress_dialogs = &suppress_dialogs_slot;
  }
}

void bind_surface(Browser& browser, content::CapabilityHost* out) {
  ShellSurface surface(browser);
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_shell_hwnd> =
                   [surface]() -> void* { return surface.hwnd_ptr(); },
               base::tag_resolver<slot_hwnd_status> =
                   [b = &browser](content::HwndStatus* status) {
                     return fill_hwnd_status(*b, status);
                   },
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

void bind_observe(Browser& browser,
                 const wchar_t* leaf,
                 content::CapabilityHost* out) {
  Browser* b = &browser;
  // Build std::function locals then move-assign onto the Host. Returning
  // lambdas funneled through host_member/tagged_tuple have AVd on
  // operator() under harness (same class as suppress_dialogs).
  if (!out) {
    return;
  }
  out->horizon.shell_status =
      [b](content::ShellStatus* status) {
        return fill_shell_status(*b, status);
      };
  out->horizon.layout_status =
      [b](content::LayoutStatus* status) {
        return fill_layout_status(*b, status);
      };
  out->horizon.wire_debug_agent = [b]() { return wire_debug_agent(*b); };
  out->horizon.debug_exec =
      [b](const std::string& line, const std::string& contains,
          const std::string& equals, const std::string& reject, int fail_rc) {
        return debug_exec(*b, line, contains, equals, reject, fail_rc);
      };
  out->horizon.console_pan_bench =
      [b, leaf]() { return console_pan_bench(*b, leaf); };
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
                     apply_ui_scenario_panels(*b, ui_mode_from_name(mode));
                     return true;
                   },
               base::tag_resolver<slot_layout_gate> =
                   [b](const std::string& mode) {
                     return run_ui_layout_gate(*b, ui_mode_from_name(mode));
                   },
               base::tag_resolver<slot_ui_present_capture> =
                   [b](const std::string& mode) {
                     return run_ui_present_capture(*b, ui_mode_from_name(mode));
                   },
           });
}

}  // namespace

void register_horizon(Browser& browser,
                      content::CapabilityHost* out,
                      const wchar_t* mark_leaf) {
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : kUiMarkLeaf;
  bind_chrome(browser, leaf, out);
  bind_surface(browser, out);
  bind_ui_slots(browser, out);
  bind_observe(browser, leaf, out);
}

}  // namespace detail
}  // namespace app
