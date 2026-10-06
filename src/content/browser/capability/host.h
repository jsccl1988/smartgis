// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HOST_H_
#define CONTENT_BROWSER_CAPABILITY_HOST_H_

#include <functional>
#include <string>
#include <vector>

#include "content/public/map_layer_types.h"

namespace content {

// Callback bag for shared harness / DebugAgent scenario verbs.
// Filled by the shell (`app/views/il.runtime`); content must not depend on
// `app::Browser`. Same pattern as DebugAgentHost.
struct CapabilityHost {
  // Message-loop pump for |ms| milliseconds.
  std::function<void(int ms)> pump;

  // Append a sidecar mark token (suite-specific leaf chosen by filler).
  std::function<void(const std::string& token)> mark;

  // Shell / catalog navigation.
  std::function<void(int index)> select_map_tab;
  std::function<void(int index)> catalog_tab;
  std::function<void(int index)> inspector_tab;

  // Shell HWND as opaque pointer (HWND on Win32). Capture/binders may use
  // it; language backends must call the inject slots below instead.
  std::function<void*()> shell_hwnd;

  // HWND inject (Win32 PostMessage). |button| 0=left 1=right; |clicks| 1 or 2.
  std::function<bool()> require_hwnd;
  std::function<bool(int x, int y, int button, int clicks)> post_click;
  std::function<bool(int x0, int y0, int x1, int y1)> post_drag;
  std::function<bool(int x, int y, int delta)> post_wheel;
  std::function<bool(const std::vector<int>& xs, const std::vector<int>& ys)>
      post_path;
  // |down| true = WM_KEYDOWN, false = WM_KEYUP (chord modifiers).
  std::function<bool(unsigned vk, bool down)> post_key;
  std::function<unsigned(const std::string& name)> vk_from_name;

  // Map edit ViewHost input.
  std::function<bool(const InputEvent&)> dispatch_edit_input;

  // Map / document session.
  std::function<bool(int timeout_ms)> wait_map_ready;
  std::function<bool(bool write_stub_if_missing)> load_china_sample;
  std::function<void()> detach_maps;
  std::function<void()> stop_map_present_timers;
  std::function<void()> resume_map_present_timers;

  // Window horizon (activate / resize). |w|/|h| used when action is resize.
  std::function<bool(const std::string& action, int w, int h)> window;
  std::function<bool(unsigned vk)> key;

  // Truncate the sidecar mark file before a suite script.
  std::function<void()> clear_marks;

  // Digitize / tool stack (Views edit host).
  std::function<bool()> edit_host_ready;
  std::function<bool(const std::string& command_id)> run_tool;
  std::function<std::string()> current_tool_id;
  // |kind|: "point" | "linestring" | "polygon"; |min_points| inclusive.
  std::function<bool(const std::string& kind, int min_points)> expect_last_geom;

  // Browse stress: stop present timer, pan+wheel bursts, assert RMB not handled.
  std::function<bool(int count)> browse_stress;
  // Wheel-to-cursor must change view scale; optional ortho camera check.
  std::function<bool(int x, int y)> expect_wheel_cursor;

  // Domain showcase bodies (legacy coarse entry; prefer atomic verbs in .il).
  // |mode|: "china" | "align" | "orthogrid"
  std::function<bool(const std::string& mode)> map2d_run;
  // |mode|: "land" | "ocean" | "full" | "coast"
  std::function<bool(const std::string& mode)> atmosphere_run;
  // Product scenario command id (e.g. mine.scenario.showcase). Chrome binds
  // this to dispatch_plugin_command so HarnessShell is attached only for the
  // call — not the removed PluginShowcaseMode plugin_run kitchen sink.
  std::function<bool(const std::string& command_id)> run_plugin_command;
  // Plugin ProcessingPool: id + JSON args (empty object ok).
  // Product showcases use run_processing(id) — not a horizon product switch.
  std::function<bool(const std::string& id, const std::string& args_json)>
      run_processing;
  // --- Wave 2 atomic verbs (file-driven suite bodies) ---
  // |kind|: "plugin" | "data" | "harness" — resolve under out/data or harness tree.
  // Writes absolute UTF-8 path into |out_path|.
  std::function<bool(const std::string& kind, const std::string& leaf,
                     std::string* out_path)>
      resolve_data;
  // |leaf| under exe/captures/ (created as needed).
  std::function<bool(const std::string& leaf, std::string* out_path)>
      capture_path;
  // Relative to exe dir (may include "..").
  std::function<bool(const std::string& rel, std::string* out_path)>
      sidecar_path;
  std::function<bool()> doc_clear;
  std::function<bool()> fit_extent;
  // |frame|: shell china_product | unit_square | document_extent, or a
  // PluginHost contribute_export_frame id (product extents are not horizon).
  std::function<bool(const std::string& leaf, const std::string& frame)>
      export_bmp;
  std::function<bool(bool on)> suppress_dialogs;
  std::function<bool()> require_plugins;
  // Load MapLibre style JSON from UTF-8 path into the document.
  std::function<bool(const std::string& path_utf8)> apply_style_file;
  // Open a vector path (GPKG / GeoJSON / …) into the map document.
  std::function<bool(const std::string& path_utf8)> open_map;
  std::function<bool()> invalidate_map2d;

  // Plugin ResultPlayback (ticks "*.present_frame" processing).
  std::function<bool(int index)> analysis_set_frame;
  std::function<int(const std::string& dir_leaf)> analysis_export_frames;

  // Local HTML report pack (Report dock / WebView2).
  std::function<bool(const std::string& report_dir)> open_report;
  std::function<bool(const std::string& json)> post_to_report;

  // HWND / chrome / console atoms (suite order lives in harness.il / console.il).
  // |face|: "map" | "scene". |want_frame| 1 requires SharedSurface present.
  std::function<int(const std::string& face, int timeout_ms, int want_frame)>
      wait_viewport;
  std::function<int()> expect_shell_tree;
  std::function<int()> expect_scene_visible;
  std::function<int()> expect_orbit_moved;
  std::function<int()> expect_layout_bounds;
  std::function<int()> expect_map_hwnd_sync;
  std::function<int(const std::string& id)> activate_tool;
  std::function<int()> wire_debug_agent;
  std::function<int(const std::string& line, const std::string& contains,
                    const std::string& equals, const std::string& reject,
                    int fail_rc)>
      debug_exec;
  std::function<int()> console_pan_bench;

  std::function<bool()> apply_ui_theme;
  std::function<bool(const std::string& mode)> ensure_china_map;
  std::function<bool(const std::string& mode)> apply_scenario_panels;
  std::function<int(const std::string& mode)> layout_gate;
  std::function<int(const std::string& mode)> ui_present_capture;
  std::function<bool()> fps_bench;
  std::function<bool(const std::string& leaf)> capture_shell_bmp;

  // Non-zero when a gate/verb wants a specific process exit (IL maps false).
  int fail_rc = 0;

  // Browse still face: "map2d" (software export) or "scene3d" (hypsometric).
  // Known faces return true after the attempt; the still writer records a miss.
  // Appended after fail_rc so older translation units keep the fail_rc offset.
  std::function<bool(const std::string& face)> capture_browse_still;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HOST_H_
