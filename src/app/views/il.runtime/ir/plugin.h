// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_PLUGIN_H_
#define IL_RUNTIME_IR_PLUGIN_H_

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Plugin and report slots. Variable binding ($var / as=) stays in the
// language backend; these calls take already-resolved strings.

inline bool run_processing(content::CapabilityHost& host,
                           std::string_view id,
                           std::string_view args_json) {
  return !id.empty() && host.run_processing &&
         host.run_processing(std::string(id), std::string(args_json));
}

inline bool run_plugin_command(content::CapabilityHost& host,
                               std::string_view id) {
  return !id.empty() && host.run_plugin_command &&
         host.run_plugin_command(std::string(id));
}

inline bool resolve_data(content::CapabilityHost& host,
                         std::string_view kind,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.resolve_data &&
         host.resolve_data(std::string(kind), std::string(leaf), out_path) &&
         out_path && !out_path->empty();
}

inline bool capture_path(content::CapabilityHost& host,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.capture_path &&
         host.capture_path(std::string(leaf), out_path) && out_path &&
         !out_path->empty();
}

inline bool sidecar_path(content::CapabilityHost& host,
                         std::string_view rel,
                         std::string* out_path) {
  return host.sidecar_path &&
         host.sidecar_path(std::string(rel), out_path) && out_path &&
         !out_path->empty();
}

inline bool require_plugins(content::CapabilityHost& host) {
  return host.require_plugins && host.require_plugins();
}

inline bool analysis_set_frame(content::CapabilityHost& host, int index) {
  return host.analysis_set_frame && host.analysis_set_frame(index);
}

inline bool analysis_export_frames(content::CapabilityHost& host,
                                   std::string_view dir) {
  return host.analysis_export_frames &&
         host.analysis_export_frames(std::string(dir)) > 0;
}

inline bool open_report(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.open_report &&
         host.open_report(std::string(path));
}

inline bool post_to_report(content::CapabilityHost& host,
                           std::string_view json) {
  return host.post_to_report && host.post_to_report(std::string(json));
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_PLUGIN_H_
