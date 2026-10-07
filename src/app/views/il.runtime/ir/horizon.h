// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_HORIZON_H_
#define IL_RUNTIME_IR_HORIZON_H_

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/views/il.runtime/ir/fail.h"

namespace app {
namespace ir {

// Shell clock, tabs, marks, HWND inject, and shell facts. Only host.horizon.
// require_hwnd also asks the view lane to detach when the shell window is gone.

inline void pump(content::CapabilityHost& host, int ms) {
  if (host.horizon.pump) {
    host.horizon.pump(ms);
  }
}

inline void mark(content::CapabilityHost& host, std::string_view token) {
  if (host.horizon.mark) {
    host.horizon.mark(std::string(token));
  }
}

inline void clear_marks(content::CapabilityHost& host) {
  if (host.horizon.clear_marks) {
    host.horizon.clear_marks();
  }
}

inline void select_view_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.select_view_tab) {
    host.horizon.select_view_tab(index);
  }
}

inline void catalog_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.catalog_tab) {
    host.horizon.catalog_tab(index);
  }
}

inline void inspector_tab(content::CapabilityHost& host, int index) {
  if (host.horizon.inspector_tab) {
    host.horizon.inspector_tab(index);
  }
}

inline void* shell_hwnd(content::CapabilityHost& host) {
  return host.horizon.shell_hwnd ? host.horizon.shell_hwnd() : nullptr;
}

inline void watch_horizon_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::HorizonFact&)> cb) {
  host.horizon.on_app_event = std::move(cb);
}

inline bool read_shell_status(content::CapabilityHost& host,
                              content::ShellStatus* out) {
  if (!out || !host.horizon.shell_status || !host.horizon.shell_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.shell = *out;
    host.horizon.on_app_event("shell.tree", fact);
  }
  return true;
}

inline bool read_layout_status(content::CapabilityHost& host,
                               content::LayoutStatus* out) {
  if (!out || !host.horizon.layout_status ||
      !host.horizon.layout_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.layout = *out;
    host.horizon.on_app_event("shell.layout", fact);
  }
  return true;
}

inline bool read_hwnd_status(content::CapabilityHost& host,
                             content::HwndStatus* out) {
  if (!out || !host.horizon.hwnd_status || !host.horizon.hwnd_status(out)) {
    return false;
  }
  if (host.horizon.on_app_event) {
    content::HorizonFact fact;
    fact.hwnd = *out;
    host.horizon.on_app_event("shell.hwnd", fact);
  }
  return true;
}

// Missing HWND detaches views and records exit code 2.
inline bool require_hwnd(content::CapabilityHost& host) {
  content::HwndStatus status;
  if (!read_hwnd_status(host, &status)) {
    return false;
  }
  if (status.alive) {
    return true;
  }
  if (host.view.detach_views) {
    host.view.detach_views();
  }
  host.fail_rc = 2;
  return false;
}

inline bool window(content::CapabilityHost& host,
                   std::string_view action,
                   int w,
                   int h) {
  return host.horizon.window && host.horizon.window(std::string(action), w, h);
}

inline unsigned vk_from_name(content::CapabilityHost& host,
                             std::string_view name) {
  if (!host.horizon.vk_from_name || name.empty()) {
    return 0;
  }
  return host.horizon.vk_from_name(std::string(name));
}

inline bool key(content::CapabilityHost& host, unsigned vk) {
  return host.horizon.key && host.horizon.key(vk);
}

inline bool post_key(content::CapabilityHost& host, unsigned vk, bool down) {
  return host.horizon.post_key && host.horizon.post_key(vk, down);
}

inline bool post_click(content::CapabilityHost& host,
                       int x,
                       int y,
                       int button,
                       int clicks) {
  return host.horizon.post_click &&
         host.horizon.post_click(x, y, button, clicks);
}

inline bool post_drag(content::CapabilityHost& host,
                      int x0,
                      int y0,
                      int x1,
                      int y1) {
  return host.horizon.post_drag && host.horizon.post_drag(x0, y0, x1, y1);
}

inline bool post_wheel(content::CapabilityHost& host, int x, int y, int delta) {
  return host.horizon.post_wheel && host.horizon.post_wheel(x, y, delta);
}

inline bool post_path(content::CapabilityHost& host,
                      const std::vector<int>& xs,
                      const std::vector<int>& ys) {
  return xs.size() >= 2 && xs.size() == ys.size() && host.horizon.post_path &&
         host.horizon.post_path(xs, ys);
}

inline bool suppress_dialogs(content::CapabilityHost& host, bool on) {
  return host.horizon.suppress_dialogs && host.horizon.suppress_dialogs(on);
}

// Contents tree: root, columns, catalog LayerTree. 4 / 5 / 6.
inline bool expect_shell_tree(content::CapabilityHost& host) {
  content::ShellStatus status;
  if (!read_shell_status(host, &status)) {
    return false;
  }
  if (!status.root_present || status.child_count < 3) {
    return apply_rc(host, 4);
  }
  if (!status.columns_present || status.column_count < 2) {
    return apply_rc(host, 5);
  }
  if (!status.catalog_present || !status.layer_tree) {
    return apply_rc(host, 6);
  }
  return true;
}

// Layout smoke. Violations detach the view and exit 30.
inline bool expect_layout_bounds(content::CapabilityHost& host) {
  content::LayoutStatus status;
  if (!read_layout_status(host, &status)) {
    return false;
  }
  if (status.violation_count > 0) {
    mark(host, "layout-fail");
    if (host.view.detach_views) {
      host.view.detach_views();
    }
    return apply_rc(host, 30);
  }
  return true;
}

inline bool wire_debug_agent(content::CapabilityHost& host) {
  return host.horizon.wire_debug_agent &&
         apply_rc(host, host.horizon.wire_debug_agent());
}

inline bool debug_exec(content::CapabilityHost& host,
                       std::string_view line,
                       std::string_view contains,
                       std::string_view equals,
                       std::string_view reject,
                       int fail_rc) {
  if (line.empty() || !host.horizon.debug_exec) {
    return false;
  }
  return apply_rc(host, host.horizon.debug_exec(
                            std::string(line), std::string(contains),
                            std::string(equals), std::string(reject), fail_rc));
}

inline bool console_pan_bench(content::CapabilityHost& host) {
  return host.horizon.console_pan_bench &&
         apply_rc(host, host.horizon.console_pan_bench());
}

inline bool apply_ui_theme(content::CapabilityHost& host) {
  return host.horizon.apply_ui_theme && host.horizon.apply_ui_theme();
}

inline bool apply_scenario_panels(content::CapabilityHost& host,
                                  std::string_view mode) {
  return host.horizon.apply_scenario_panels &&
         host.horizon.apply_scenario_panels(std::string(mode));
}

inline bool layout_gate(content::CapabilityHost& host, std::string_view mode) {
  if (!host.horizon.layout_gate) {
    return false;
  }
  return apply_rc(host, host.horizon.layout_gate(std::string(mode)));
}

inline bool ui_present_capture(content::CapabilityHost& host,
                               std::string_view mode) {
  if (!host.horizon.ui_present_capture) {
    return false;
  }
  return apply_rc(host, host.horizon.ui_present_capture(std::string(mode)));
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_HORIZON_H_
