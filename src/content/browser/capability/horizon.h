// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HORIZON_H_
#define CONTENT_BROWSER_CAPABILITY_HORIZON_H_

#include <functional>
#include <string>
#include <vector>

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
  std::function<void(int ms)> pump;
  std::function<void(const std::string& token)> mark;
  std::function<void()> clear_marks;

  std::function<void(int index)> select_map_tab;
  std::function<void(int index)> catalog_tab;
  std::function<void(int index)> inspector_tab;

  // Shell HWND as an opaque pointer (HWND on Win32).
  std::function<void*()> shell_hwnd;
  // |button| 0=left 1=right; |clicks| 1 or 2.
  std::function<bool(int x, int y, int button, int clicks)> post_click;
  std::function<bool(int x0, int y0, int x1, int y1)> post_drag;
  std::function<bool(int x, int y, int delta)> post_wheel;
  std::function<bool(const std::vector<int>& xs, const std::vector<int>& ys)>
      post_path;
  // |down| true = WM_KEYDOWN, false = WM_KEYUP.
  std::function<bool(unsigned vk, bool down)> post_key;
  std::function<unsigned(const std::string& name)> vk_from_name;
  // |w|/|h| used when action is resize.
  std::function<bool(const std::string& action, int w, int h)> window;
  std::function<bool(unsigned vk)> key;
  std::function<bool(bool on)> suppress_dialogs;

  // IL installs this. Bind must not overwrite it.
  std::function<void(const std::string& name, const HorizonFact& fact)>
      on_app_event;
  // Snapshots. None decide pass or fail.
  std::function<bool(ShellStatus* out)> shell_status;
  std::function<bool(LayoutStatus* out)> layout_status;
  std::function<bool(HwndStatus* out)> hwnd_status;

  std::function<int()> wire_debug_agent;
  std::function<int(const std::string& line, const std::string& contains,
                    const std::string& equals, const std::string& reject,
                    int fail_rc)>
      debug_exec;
  std::function<int()> console_pan_bench;

  std::function<bool()> apply_ui_theme;
  std::function<bool(const std::string& mode)> apply_scenario_panels;
  std::function<int(const std::string& mode)> layout_gate;
  std::function<int(const std::string& mode)> ui_present_capture;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HORIZON_H_
