// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/cmd/agent_ask.h"

#include <cctype>
#include <sstream>

#include "content/browser/debug/cmd/agent_diag.h"
#include "content/browser/debug/cmd/agent_gis_cmd.h"
#include "content/browser/debug/schema/agent_schema.h"

namespace content {
namespace detail {
namespace {

std::string to_lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool contains(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

}  // namespace

bool exec_ask_command(const std::string& line,
                      const DebugAgentHost& host,
                      DebugAgent* agent,
                      std::string* output) {
  if (!output || line.rfind(":ask", 0) != 0) {
    return false;
  }
  std::string q = line.size() > 4 ? line.substr(4) : std::string();
  while (!q.empty() && (q.front() == ' ' || q.front() == '\t')) {
    q.erase(q.begin());
  }
  if (q.empty()) {
    *output =
        "Ask stub (local tools only; remote LLM not wired). Try: "
        ":ask layers | extent | fps | tree | diag | help | record";
    return true;
  }
  const std::string lq = to_lower(q);

  if (contains(lq, "help") || contains(lq, "schema") ||
      contains(lq, "method")) {
    *output = agent_help_text();
    return true;
  }
  if (contains(lq, "layer")) {
    std::string map_out;
    if (exec_gis_command(":layers", host, &map_out)) {
      *output = map_out;
      return true;
    }
  }
  if (contains(lq, "extent") || contains(lq, "bbox")) {
    std::string map_out;
    if (exec_gis_command(":extent", host, &map_out)) {
      *output = map_out;
      return true;
    }
  }
  if (contains(lq, "fps") || contains(lq, "overlay") ||
      contains(lq, "present") || contains(lq, "rhi")) {
    if (host.ui_overlay_stats) {
      *output = host.ui_overlay_stats();
      return true;
    }
    *output = "overlay_stats host not bound";
    return true;
  }
  if (contains(lq, "tree") || contains(lq, "ui dump")) {
    if (host.ui_dump_tree) {
      *output = host.ui_dump_tree();
      return true;
    }
    *output = "ui host not bound";
    return true;
  }
  if (contains(lq, "diag") || contains(lq, "pack")) {
    *output = build_diag_pack(host, 80, false);
    return true;
  }
  if (contains(lq, "record")) {
    if (!agent) {
      *output = "agent unavailable";
      return true;
    }
    agent->set_record_enabled(true);
    *output = "record enabled; use :record poll / record.poll";
    return true;
  }
  if (contains(lq, "script") || contains(lq, "repro") ||
      contains(lq, "interact")) {
    *output =
        "Use :script <path.il> or script.run {path}. "
        "Ask stub does not invent IL paths.";
    return true;
  }

  std::ostringstream oss;
  oss << "Ask stub: no local tool match for \"" << q
      << "\". Remote LLM backend is stubbed — see rpc.methods / :help json. "
         "Matched keywords: layers, extent, fps, tree, diag, help, record.";
  *output = oss.str();
  return true;
}

}  // namespace detail
}  // namespace content
