// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HORIZON_H_
#define CONTENT_BROWSER_CAPABILITY_HORIZON_H_

#include <concepts>
#include <string>
#include <vector>

#include "content/browser/capability/op.h"

namespace content {

// Contents tree counts. Facts only: IL decides 4 / 5 / 6.
struct ShellStatus {
  int root_present = 0;
  int child_count = 0;
  int columns_present = 0;
  int column_count = 0;
  int catalog_present = 0;
  int layer_tree = 0;
};

// Layout smoke counts. Facts only: IL decides 30.
struct LayoutStatus {
  int violation_count = 0;
  int overlap_count = 0;
};

// Shell HWND liveness. Facts only: IL decides require_hwnd (exit 2).
struct HwndStatus {
  int alive = 0;
};

// Payload for |HorizonCapability::on_app_event|. |name| selects which
// snapshot was just read; the other fields stay default.
struct HorizonFact {
  ShellStatus shell;
  LayoutStatus layout;
  HwndStatus hwnd;
};

// Horizon lane. Chrome tabs, HWND inject, marks, and UI/debug actions.
// Shell, layout, and HWND checks are not gates: IL registers |on_app_event|
// and reads |shell_status| / |layout_status| / |hwnd_status|. Not a GIS
// content object; the shell fills these because content must not depend on
// app::Browser.
struct HorizonCapability {
  detail::Op<void(int ms)> pump;
  detail::Op<void(const std::string& token)> mark;
  detail::Op<void()> clear_marks;

  detail::Op<void(int index)> select_view_tab;
  detail::Op<void(int index)> catalog_tab;
  detail::Op<void(int index)> inspector_tab;

  // Shell HWND as an opaque pointer (HWND on Win32).
  detail::Op<void*()> shell_hwnd;
  // |button| 0=left 1=right; |clicks| 1 or 2.
  detail::Op<bool(int x, int y, int button, int clicks)> post_click;
  detail::Op<bool(int x0, int y0, int x1, int y1)> post_drag;
  detail::Op<bool(int x, int y, int delta)> post_wheel;
  detail::Op<bool(const std::vector<int>& xs, const std::vector<int>& ys)>
      post_path;
  // |down| true = WM_KEYDOWN, false = WM_KEYUP.
  detail::Op<bool(unsigned vk, bool down)> post_key;
  detail::Op<unsigned(const std::string& name)> vk_from_name;
  // |w|/|h| used when action is resize.
  detail::Op<bool(const std::string& action, int w, int h)> window;
  detail::Op<bool(unsigned vk)> key;
  detail::Op<bool(bool on)> suppress_dialogs;

  // IL installs this. Bind must not overwrite it.
  detail::Op<void(const std::string& name, const HorizonFact& fact)>
      on_app_event;
  // Snapshots. None decide pass or fail.
  detail::Op<bool(ShellStatus* out)> shell_status;
  detail::Op<bool(LayoutStatus* out)> layout_status;
  detail::Op<bool(HwndStatus* out)> hwnd_status;

  detail::Op<int()> wire_debug_agent;
  detail::Op<int(const std::string& line, const std::string& contains,
                    const std::string& equals, const std::string& reject,
                    int fail_rc)>
      debug_exec;
  detail::Op<int()> console_pan_bench;

  detail::Op<bool()> apply_ui_theme;
  detail::Op<bool(const std::string& mode)> apply_scenario_panels;
  detail::Op<int(const std::string& mode)> layout_gate;
  detail::Op<int(const std::string& mode)> ui_present_capture;
};

template <typename T>
concept horizon_capability = requires(T& t,
                                      const std::string& s,
                                      int n,
                                      unsigned vk,
                                      bool down,
                                      const std::vector<int>& xs,
                                      ShellStatus* shell,
                                      LayoutStatus* layout,
                                      HwndStatus* hwnd) {
  { t.pump(n) };
  { t.mark(s) };
  { t.clear_marks() };
  { t.select_view_tab(n) };
  { t.catalog_tab(n) };
  { t.inspector_tab(n) };
  { t.shell_hwnd() } -> std::convertible_to<void*>;
  { t.post_click(n, n, n, n) } -> std::convertible_to<bool>;
  { t.post_drag(n, n, n, n) } -> std::convertible_to<bool>;
  { t.post_wheel(n, n, n) } -> std::convertible_to<bool>;
  { t.post_path(xs, xs) } -> std::convertible_to<bool>;
  { t.post_key(vk, down) } -> std::convertible_to<bool>;
  { t.vk_from_name(s) } -> std::convertible_to<unsigned>;
  { t.window(s, n, n) } -> std::convertible_to<bool>;
  { t.key(vk) } -> std::convertible_to<bool>;
  { t.suppress_dialogs(down) } -> std::convertible_to<bool>;
  { t.shell_status(shell) } -> std::convertible_to<bool>;
  { t.layout_status(layout) } -> std::convertible_to<bool>;
  { t.hwnd_status(hwnd) } -> std::convertible_to<bool>;
  { t.wire_debug_agent() } -> std::convertible_to<int>;
  { t.debug_exec(s, s, s, s, n) } -> std::convertible_to<int>;
  { t.console_pan_bench() } -> std::convertible_to<int>;
  { t.apply_ui_theme() } -> std::convertible_to<bool>;
  { t.apply_scenario_panels(s) } -> std::convertible_to<bool>;
  { t.layout_gate(s) } -> std::convertible_to<int>;
  { t.ui_present_capture(s) } -> std::convertible_to<int>;
};

static_assert(horizon_capability<HorizonCapability>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HORIZON_H_
