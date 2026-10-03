// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_HOST_H_
#define CONTENT_BROWSER_CAPABILITY_HOST_H_

#include <functional>
#include <string>

#include "content/public/map_types.h"

namespace content {

// Callback bag for shared harness / DebugAgent scenario verbs.
// Filled by the shell (`app/views/shell/runtime`); content must not depend on
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

  // Shell HWND as opaque pointer (HWND on Win32). Used by DSL inject helpers.
  std::function<void*()> shell_hwnd;

  // Map edit ViewHost input.
  std::function<bool(const InputEvent&)> dispatch_edit_input;

  // Map / document session.
  std::function<bool(int timeout_ms)> wait_map_ready;
  std::function<bool(bool write_stub_if_missing)> load_china_sample;
  std::function<void()> detach_maps;
  std::function<void()> stop_map_present_timers;

  // Window chrome (activate / resize). |w|/|h| used when action is resize.
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
  // |mode|: "world3d" | "print" | "orthogrid" | …
  std::function<bool(const std::string& mode)> plugin_run;
  // Plugin ProcessingPool: id + JSON args (empty object ok).
  std::function<bool(const std::string& id, const std::string& args_json)>
      run_processing;
  // Short DebugAgent console self-test body.
  std::function<bool()> console_run;

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
  // |frame|: china_product | unit_square | document_extent | traffic_beijing |
  // flood_wuhan | world3d_tin
  std::function<bool(const std::string& leaf, const std::string& frame)>
      export_bmp;
  std::function<bool(bool on)> suppress_dialogs;
  std::function<bool()> require_plugins;
  // Load MapLibre style JSON from UTF-8 path into the document.
  std::function<bool(const std::string& path_utf8)> apply_style_file;
  std::function<bool()> invalidate_map2d;

  // AnalysisPlayback ResultPlayback (traffic/flood/orthogrid).
  std::function<bool(int index)> analysis_set_frame;
  std::function<int(const std::string& dir_leaf)> analysis_export_frames;

  // Local HTML report pack (Report dock / WebView2).
  std::function<bool(const std::string& report_dir)> open_report;
  std::function<bool(const std::string& json)> post_to_report;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_HOST_H_
