// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_PLUGIN_H_
#define CONTENT_BROWSER_CAPABILITY_PLUGIN_H_

#include <concepts>
#include <string>

#include "content/browser/capability/op.h"

namespace content {

// Plugin lane. Mirrors PluginHost: commands, processing, playback, reports,
// and path resolution the shell fills for harness scripts. Product scenario
// mode ops (map2d_run / atmosphere_run / …) live in
// plugin::register_harness_op — not as Host slots.
struct PluginCapability {
  // Product scenario command id (e.g. mine.scenario.run).
  detail::Op<bool(const std::string& command_id)> run_plugin_command;
  // Plugin ProcessingPool: id + JSON args (empty object ok).
  detail::Op<bool(const std::string& id, const std::string& args_json)> run_processing;
  detail::Op<bool()> require_plugins;

  // |kind|: "plugin" | "data" | "harness". Writes an absolute UTF-8 path.
  detail::Op<bool(const std::string& kind, const std::string& leaf, std::string* out_path)>
      resolve_data;
  // |leaf| under exe/captures/ (created as needed).
  detail::Op<bool(const std::string& leaf, std::string* out_path)> capture_path;
  // Relative to exe dir (may include "..").
  detail::Op<bool(const std::string& rel, std::string* out_path)> sidecar_path;

  // Plugin ResultPlayback (ticks "*.present_frame" processing).
  detail::Op<bool(int index)> analysis_set_frame;
  detail::Op<int(const std::string& dir_leaf)> analysis_export_frames;
  detail::Op<bool(const std::string& report_dir)> open_report;
  detail::Op<bool(const std::string& json)> post_to_report;
};

template <typename T>
concept plugin_capability = requires(T& t, const std::string& s, std::string* path, int index) {
  { t.run_plugin_command(s) } -> std::convertible_to<bool>;
  { t.run_processing(s, s) } -> std::convertible_to<bool>;
  { t.require_plugins() } -> std::convertible_to<bool>;
  { t.resolve_data(s, s, path) } -> std::convertible_to<bool>;
  { t.capture_path(s, path) } -> std::convertible_to<bool>;
  { t.sidecar_path(s, path) } -> std::convertible_to<bool>;
  { t.analysis_set_frame(index) } -> std::convertible_to<bool>;
  { t.analysis_export_frames(s) } -> std::convertible_to<int>;
  { t.open_report(s) } -> std::convertible_to<bool>;
  { t.post_to_report(s) } -> std::convertible_to<bool>;
};

static_assert(plugin_capability<PluginCapability>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_PLUGIN_H_
