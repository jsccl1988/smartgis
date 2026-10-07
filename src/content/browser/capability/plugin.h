// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_PLUGIN_H_
#define CONTENT_BROWSER_CAPABILITY_PLUGIN_H_

#include <functional>
#include <string>

namespace content {

// Plugin lane. Mirrors PluginHost: commands, processing, playback, reports,
// and path resolution the shell fills for harness scripts. Product scenario
// mode ops (map2d_run / atmosphere_run / …) live in
// plugin::register_scenario_op — not as Host slots.
struct PluginCapability {
  // Product scenario command id (e.g. mine.scenario.run).
  std::function<bool(const std::string& command_id)> run_plugin_command;
  // Plugin ProcessingPool: id + JSON args (empty object ok).
  std::function<bool(const std::string& id, const std::string& args_json)>
      run_processing;
  std::function<bool()> require_plugins;

  // |kind|: "plugin" | "data" | "harness". Writes an absolute UTF-8 path.
  std::function<bool(const std::string& kind, const std::string& leaf,
                     std::string* out_path)>
      resolve_data;
  // |leaf| under exe/captures/ (created as needed).
  std::function<bool(const std::string& leaf, std::string* out_path)>
      capture_path;
  // Relative to exe dir (may include "..").
  std::function<bool(const std::string& rel, std::string* out_path)>
      sidecar_path;

  // Plugin ResultPlayback (ticks "*.present_frame" processing).
  std::function<bool(int index)> analysis_set_frame;
  std::function<int(const std::string& dir_leaf)> analysis_export_frames;
  std::function<bool(const std::string& report_dir)> open_report;
  std::function<bool(const std::string& json)> post_to_report;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_PLUGIN_H_
