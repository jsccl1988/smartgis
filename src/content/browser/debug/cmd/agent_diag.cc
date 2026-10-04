// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/cmd/agent_diag.h"

#include <sstream>

#include "base/log/log_sink.h"
#include "content/browser/debug/wire/agent_json.h"
#include "content/browser/debug/cmd/agent_log.h"

namespace content {
namespace detail {

std::string build_diag_pack(const DebugAgentHost& host,
                            int log_n,
                            bool capture) {
  if (log_n < 0) {
    log_n = 0;
  }
  if (log_n > 2000) {
    log_n = 2000;
  }

  std::ostringstream oss;
  oss << '{';

  // Log tail.
  {
    const auto entries =
        base::log_sink().snapshot_tail(static_cast<size_t>(log_n));
    oss << "\"log\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
      if (i) {
        oss << ',';
      }
      oss << log_entry_json(entries[i]);
    }
    oss << ']';
  }

  // Extent.
  oss << ",\"extent\":\"";
  if (host.extent_string) {
    oss << json_escape(host.extent_string());
  } else {
    oss << "unavailable";
  }
  oss << '"';

  // Layers.
  oss << ",\"layers\":[";
  if (host.layer_names) {
    const auto names = host.layer_names();
    for (size_t i = 0; i < names.size(); ++i) {
      if (i) {
        oss << ',';
      }
      oss << '"' << json_escape(names[i]) << '"';
    }
  }
  oss << ']';

  // UI tree (may be large; still useful for one-shot packs).
  oss << ",\"ui_tree\":\"";
  if (host.ui_dump_tree) {
    oss << json_escape(host.ui_dump_tree());
  } else {
    oss << "unavailable";
  }
  oss << '"';

  // Overlay / present stats.
  oss << ",\"overlay_stats\":\"";
  if (host.ui_overlay_stats) {
    oss << json_escape(host.ui_overlay_stats());
  } else {
    oss << "unavailable";
  }
  oss << '"';

  // Optional shell capture path.
  oss << ",\"capture\":\"";
  if (capture && host.ui_capture_shell) {
    oss << json_escape(host.ui_capture_shell(""));
  } else if (capture) {
    oss << "unavailable";
  } else {
    oss << "";
  }
  oss << '"';

  oss << '}';
  return oss.str();
}

bool dispatch_diag_method(const std::string& method,
                          const std::string& params_json,
                          int id,
                          const DebugAgentHost& host,
                          std::string* response) {
  if (!response || method != "diag.pack") {
    return false;
  }
  int log_n = 100;
  extract_int_field(params_json, "log_n", &log_n);
  bool capture = false;
  {
    // Lightweight bool extract without full RapidJSON dependency here.
    if (params_json.find("\"capture\":true") != std::string::npos ||
        params_json.find("\"capture\": true") != std::string::npos) {
      capture = true;
    }
  }
  const std::string pack = build_diag_pack(host, log_n, capture);
  *response = ok_result(id, pack);
  return true;
}

bool exec_diag_command(const std::string& line,
                       const DebugAgentHost& host,
                       std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":diag") {
    *output = build_diag_pack(host, 100, false);
    return true;
  }
  if (line == ":diag capture") {
    *output = build_diag_pack(host, 100, true);
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
