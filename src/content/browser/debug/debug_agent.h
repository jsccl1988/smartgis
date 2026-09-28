// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_
#define CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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

  DebugAgentHost host_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_{false};
  std::atomic<int> port_{0};
  unsigned long long listen_sock_ = 0;
  std::thread accept_thread_;
  std::mutex mu_;
  std::uint64_t log_sub_id_ = 0;
  // Connected python worker socket (optional).
  unsigned long long py_sock_ = 0;
  std::mutex py_mu_;
};

// Process-wide agent used by Views Console (created on first enable).
DebugAgent& debug_agent();

// True when --debug-console, SG_DEBUG=1, or explicit start requested.
bool debug_console_env_enabled();

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_DEBUG_AGENT_H_
