// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_
#define CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "content/browser/debug/agent_policy.h"

namespace content {

// Narrow host hooks so DebugAgent does not depend on Browser widgets.
struct DebugAgentHost {
  std::function<void()> refresh_map;
  std::function<std::string()> extent_string;
  std::function<std::vector<std::string>()> layer_names;
  std::function<std::string(const std::string& name)> ui_find;
  std::function<std::string(int x, int y, int button)> ui_click;
  std::function<std::string(const std::string& utf8)> ui_type;
  std::function<std::string()> ui_dump_tree;
  std::function<std::string()> ui_overlay_stats;
  // Optional: capture shell chrome to a PNG/BMP path (forensics Mode C).
  std::function<std::string(const std::string& path)> ui_capture_shell;
  // Prefer in-process CPython when set; else OOP spawn fallback.
  std::function<std::string(const std::string& code)> py_eval;
  // Run Interact DSL / capability script (path UTF-8). Thin wrap only.
  std::function<std::string(const std::string& path_utf8)> script_run;
};

// Opt-in loopback NDJSON debug agent (log / cmd / gis / sdbd / py).
class DebugAgent {
 public:
  DebugAgent();
  ~DebugAgent();

  DebugAgent(const DebugAgent&) = delete;
  DebugAgent& operator=(const DebugAgent&) = delete;

  void set_host(DebugAgentHost host);

  // Bind 127.0.0.1 (port 0 = ephemeral, or SG_DEBUG_PORT). Returns false on
  // failure. Idempotent if already running.
  bool start();
  void stop();

  bool is_running() const { return running_.load(); }
  int port() const { return port_.load(); }

  // Process one console line (builtin :commands or py one-liner).
  std::string exec_line(const std::string& line);

  // Semantic event ring for IL recording (record.enable / record.poll).
  void set_record_enabled(bool on);
  bool record_enabled() const { return record_enabled_.load(); }
  void push_record_event(const std::string& kind, const std::string& fields_json);
  // Returns JSON array body (no wrapping) of drained events; clears buffer.
  std::string poll_record_events_json();

  // Session confirm for dangerous ops (see agent_policy).
  void confirm_dangerous_ops();
  bool dangerous_ops_allowed() const;

 private:
  void accept_loop();
  void serve_client(unsigned long long sock);
  std::string handle_request_json(const std::string& line);
  std::string dispatch_method(const std::string& method,
                              const std::string& params_json,
                              int id);
  void write_discovery_file();
  void clear_discovery_file();
  bool spawn_python_worker();
  std::string eval_python(const std::string& code);
  DebugAgentHost copy_host() const;

  DebugAgentHost host_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_{false};
  std::atomic<int> port_{0};
  unsigned long long listen_sock_ = 0;
  std::thread accept_thread_;
  mutable std::mutex mu_;
  std::uint64_t log_sub_id_ = 0;
  // Connected python worker socket (optional).
  unsigned long long py_sock_ = 0;
  std::mutex py_mu_;

  std::atomic<bool> record_enabled_{false};
  std::mutex record_mu_;
  std::vector<std::string> record_events_;
  std::int64_t record_t0_ms_ = 0;

  mutable std::mutex policy_mu_;
  detail::AgentPolicy policy_;
};

// Process-wide agent used by Views Console (created on first enable).
DebugAgent& debug_agent();

// True when --debug-console, SG_DEBUG=1, or explicit start requested.
bool debug_console_env_enabled();

// Push a semantic event when recording is enabled (no-op otherwise).
// |fields_json| is a JSON object fragment without kind, e.g. {"index":2}.
void push_record_event(const std::string& kind, const std::string& fields_json);

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_
