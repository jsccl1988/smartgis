// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_PLUGIN_H_
#define IL_RUNTIME_IR_PLUGIN_H_

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// PluginHost lane: commands, processing, playback, reports, paths.
// Variable binding ($var / as=) stays in the language backend. Only host.plugin.

inline bool map2d_run(content::CapabilityHost& host, std::string_view mode) {
  return host.plugin.map2d_run && host.plugin.map2d_run(std::string(mode));
}

inline bool atmosphere_run(content::CapabilityHost& host, std::string_view mode) {
  return host.plugin.atmosphere_run &&
         host.plugin.atmosphere_run(std::string(mode));
}

inline bool run_processing(content::CapabilityHost& host,
                           std::string_view id,
                           std::string_view args_json) {
  return !id.empty() && host.plugin.run_processing &&
         host.plugin.run_processing(std::string(id), std::string(args_json));
}

inline bool run_plugin_command(content::CapabilityHost& host,
                               std::string_view id) {
  return !id.empty() && host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command(std::string(id));
}

inline bool resolve_data(content::CapabilityHost& host,
                         std::string_view kind,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.plugin.resolve_data &&
         host.plugin.resolve_data(std::string(kind), std::string(leaf),
                                  out_path) &&
         out_path && !out_path->empty();
}

inline bool capture_path(content::CapabilityHost& host,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.plugin.capture_path &&
         host.plugin.capture_path(std::string(leaf), out_path) && out_path &&
         !out_path->empty();
}

inline bool sidecar_path(content::CapabilityHost& host,
                         std::string_view rel,
                         std::string* out_path) {
  return host.plugin.sidecar_path &&
         host.plugin.sidecar_path(std::string(rel), out_path) && out_path &&
         !out_path->empty();
}

inline bool require_plugins(content::CapabilityHost& host) {
  return host.plugin.require_plugins && host.plugin.require_plugins();
}

inline bool analysis_set_frame(content::CapabilityHost& host, int index) {
  return host.plugin.analysis_set_frame && host.plugin.analysis_set_frame(index);
}

inline bool analysis_export_frames(content::CapabilityHost& host,
                                   std::string_view dir) {
  return host.plugin.analysis_export_frames &&
         host.plugin.analysis_export_frames(std::string(dir)) > 0;
}

inline bool open_report(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.plugin.open_report &&
         host.plugin.open_report(std::string(path));
}

inline bool post_to_report(content::CapabilityHost& host,
                           std::string_view json) {
  return host.plugin.post_to_report &&
         host.plugin.post_to_report(std::string(json));
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_PLUGIN_H_
