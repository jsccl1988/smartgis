// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/debug_agent.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "base/core/log.h"
#include "base/log/log_sink.h"
#include "base/process/switches.h"
#include "content/browser/debug/cmd/agent_ask.h"
#include "content/browser/debug/cmd/agent_diag.h"
#include "content/browser/debug/cmd/agent_harness.h"
#include "content/browser/debug/cmd/agent_log.h"
#include "content/browser/debug/cmd/agent_map_cmd.h"
#include "content/browser/debug/cmd/agent_py.h"
#include "content/browser/debug/cmd/agent_record.h"
#include "content/browser/debug/cmd/agent_sdbd.h"
#include "content/browser/debug/cmd/agent_ui.h"
#include "content/browser/debug/policy/agent_policy.h"
#include "content/browser/debug/schema/agent_schema.h"
#include "content/browser/debug/wire/agent_json.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#pragma comment(lib, "ws2_32.lib")

namespace content {
namespace {

std::once_flag g_wsa_once;

void ensure_wsa() {
  std::call_once(g_wsa_once, [] {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
  });
}

std::string discovery_path() {
  char buf[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, buf);
  if (n == 0 || n > MAX_PATH) {
    return "smartgis-debug.json";
  }
  return std::string(buf) + "smartgis-debug.json";
}

}  // namespace

DebugAgent::DebugAgent() = default;

DebugAgent::~DebugAgent() {
  stop();
}

void DebugAgent::set_host(DebugAgentHost host) {
  std::lock_guard<std::mutex> lock(mu_);
  host_ = std::move(host);
}

DebugAgentHost DebugAgent::copy_host() const {
  std::lock_guard<std::mutex> lock(mu_);
  return host_;
}

std::string DebugAgent::eval_python(const std::string& code) {
  DebugAgentHost host = copy_host();
  if (host.py_eval) {
    return host.py_eval(code);
  }
  return detail::run_python_code_oop(code);
}

bool DebugAgent::start() {
  if (running_.load()) {
    return true;
  }
  ensure_wsa();

  int want_port = 0;
  if (const char* env = base::switch_cstr("debug-port")) {
    want_port = std::atoi(env);
  }

  SOCKET listen_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_sock == INVALID_SOCKET) {
    LOGGING(LOG_ERROR, "DebugAgent: socket() failed");
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<u_short>(want_port));
  if (::bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    LOGGING(LOG_ERROR, "DebugAgent: bind() failed");
    closesocket(listen_sock);
    return false;
  }
  if (::listen(listen_sock, 8) != 0) {
    LOGGING(LOG_ERROR, "DebugAgent: listen() failed");
    closesocket(listen_sock);
    return false;
  }
  int addrlen = sizeof(addr);
  if (getsockname(listen_sock, reinterpret_cast<sockaddr*>(&addr), &addrlen) ==
      0) {
    port_.store(ntohs(addr.sin_port));
  }

  listen_sock_ = static_cast<unsigned long long>(listen_sock);
  stop_.store(false);
  running_.store(true);
  write_discovery_file();

  log_sub_id_ = base::log_sink().subscribe([this](const base::LogEntry& e) {
    // Subscribers on connections are handled per-client; global sink still
    // feeds panel via direct subscribe. Agent push is per-session in serve.
    (void)e;
    (void)this;
  });

  accept_thread_ = std::thread([this] { accept_loop(); });
  LOGGING(LOG_INFO, "DebugAgent listening on 127.0.0.1:%d", port_.load());
  return true;
}

void DebugAgent::stop() {
  if (!running_.exchange(false)) {
    return;
  }
  stop_.store(true);
  if (listen_sock_) {
    closesocket(static_cast<SOCKET>(listen_sock_));
    listen_sock_ = 0;
  }
  {
    std::lock_guard<std::mutex> lock(py_mu_);
    if (py_sock_) {
      closesocket(static_cast<SOCKET>(py_sock_));
      py_sock_ = 0;
    }
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  if (log_sub_id_) {
    base::log_sink().unsubscribe(log_sub_id_);
    log_sub_id_ = 0;
  }
  clear_discovery_file();
  port_.store(0);
}

void DebugAgent::write_discovery_file() {
  std::ofstream out(discovery_path(), std::ios::binary);
  out << "{\"host\":\"127.0.0.1\",\"port\":" << port_.load()
      << ",\"pid\":" << GetCurrentProcessId() << "}\n";
}

void DebugAgent::clear_discovery_file() {
  std::error_code ec;
  std::filesystem::remove(discovery_path(), ec);
}

void DebugAgent::accept_loop() {
  SOCKET listen_sock = static_cast<SOCKET>(listen_sock_);
  while (!stop_.load()) {
    sockaddr_in peer{};
    int peer_len = sizeof(peer);
    SOCKET client =
        ::accept(listen_sock, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (client == INVALID_SOCKET) {
      break;
    }
    std::thread([this, client] {
      serve_client(static_cast<unsigned long long>(client));
    }).detach();
  }
}

void DebugAgent::serve_client(unsigned long long sock_u) {
  SOCKET sock = static_cast<SOCKET>(sock_u);
  std::string buf;
  char tmp[1024];
  std::uint64_t sub_id = 0;
  auto send_all = [&](const std::string& s) {
    const char* p = s.c_str();
    int left = static_cast<int>(s.size());
    while (left > 0) {
      const int n = ::send(sock, p, left, 0);
      if (n <= 0) {
        return false;
      }
      p += n;
      left -= n;
    }
    return true;
  };

  while (!stop_.load()) {
    const int n = ::recv(sock, tmp, sizeof(tmp), 0);
    if (n <= 0) {
      break;
    }
    buf.append(tmp, tmp + n);
    for (;;) {
      const auto nl = buf.find('\n');
      if (nl == std::string::npos) {
        break;
      }
      std::string line = buf.substr(0, nl);
      buf.erase(0, nl + 1);
      if (line.empty()) {
        continue;
      }
      // Strip CR.
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      std::string method;
      detail::extract_string_field(line, "method", &method);
      if (method == "log.subscribe") {
        int id = 0;
        detail::extract_int_field(line, "id", &id);
        if (sub_id) {
          base::log_sink().unsubscribe(sub_id);
        }
        sub_id = base::log_sink().subscribe([&](const base::LogEntry& e) {
          std::ostringstream oss;
          oss << "{\"event\":\"log\",\"entry\":" << detail::log_entry_json(e)
              << "}\n";
          send_all(oss.str());
        });
        send_all(detail::ok_result(id, "{}") + "\n");
        continue;
      }
      if (method == "py.register") {
        int id = 0;
        detail::extract_int_field(line, "id", &id);
        {
          std::lock_guard<std::mutex> lock(py_mu_);
          if (py_sock_) {
            closesocket(static_cast<SOCKET>(py_sock_));
          }
          py_sock_ = sock_u;
        }
        send_all(detail::ok_result(id, "{\"registered\":true}") + "\n");
        // Keep this connection as python channel; exit read loop ownership
        // transferred — still read jobs responses later via py_mu.
        // For v1 spawn-per-eval, registration is optional bookkeeping.
        continue;
      }
      const std::string resp = handle_request_json(line);
      if (!send_all(resp + "\n")) {
        break;
      }
    }
  }
  if (sub_id) {
    base::log_sink().unsubscribe(sub_id);
  }
  {
    std::lock_guard<std::mutex> lock(py_mu_);
    if (py_sock_ == sock_u) {
      py_sock_ = 0;
    }
  }
  closesocket(sock);
}

std::string DebugAgent::handle_request_json(const std::string& line) {
  rapidjson::Document doc;
  doc.Parse(line.c_str());
  int id = 0;
  std::string method;
  if (!doc.HasParseError() && doc.IsObject()) {
    const auto id_it = doc.FindMember("id");
    if (id_it != doc.MemberEnd() && id_it->value.IsNumber()) {
      id = id_it->value.GetInt();
    }
    const auto method_it = doc.FindMember("method");
    if (method_it != doc.MemberEnd() && method_it->value.IsString()) {
      method.assign(method_it->value.GetString(),
                    method_it->value.GetStringLength());
    }
  }
  if (method.empty()) {
    return detail::err_result(id, "missing method");
  }
  std::string params = "{}";
  if (!doc.HasParseError() && doc.IsObject()) {
    const auto params_it = doc.FindMember("params");
    if (params_it != doc.MemberEnd() && params_it->value.IsObject()) {
      rapidjson::StringBuffer buf;
      rapidjson::Writer<rapidjson::StringBuffer> w(buf);
      params_it->value.Accept(w);
      params.assign(buf.GetString(), buf.GetSize());
    }
  }
  return dispatch_method(method, params, id);
}

std::string DebugAgent::dispatch_method(const std::string& method,
                                        const std::string& params_json,
                                        int id) {
  using detail::err_result;
  using detail::json_escape;
  using detail::ok_result;

  if (method == "ping") {
    return ok_result(id, "{\"pong\":true}");
  }
  if (method == "rpc.methods") {
    return ok_result(id, detail::agent_methods_schema_json());
  }
  if (method == "rpc.confirm") {
    confirm_dangerous_ops();
    return ok_result(id, "{\"confirmed\":true}");
  }

  {
    std::lock_guard<std::mutex> lock(policy_mu_);
    if (!policy_.allow_rpc(method, params_json)) {
      detail::AgentPolicy::audit("deny", method);
      return err_result(
          id,
          "needs_confirm: call rpc.confirm, pass confirm:true, or set "
          "SG_DEBUG_ALLOW=1");
    }
  }
  if (detail::AgentPolicy::is_dangerous_method(method)) {
    detail::AgentPolicy::audit("rpc", method);
  }

  std::string response;
  if (detail::dispatch_log_method(method, params_json, id, &response)) {
    return response;
  }
  if (method == "cmd.exec") {
    std::string line;
    detail::extract_string_field(params_json, "line", &line);
    const std::string output = exec_line(line);
    return ok_result(id, std::string("{\"output\":\"") + json_escape(output) +
                             "\"}");
  }
  if (detail::dispatch_sdbd_method(method, params_json, id, &response)) {
    return response;
  }
  {
    const detail::PyEvalFn eval = [this](const std::string& code) {
      return eval_python(code);
    };
    if (detail::dispatch_py_method(method, params_json, id, eval, &response)) {
      return response;
    }
  }
  if (method == "shutdown") {
    std::thread([this] { stop(); }).detach();
    return ok_result(id, "{}");
  }
  {
    const DebugAgentHost host = copy_host();
    if (detail::dispatch_ui_method(method, params_json, id, host, &response)) {
      return response;
    }
    if (detail::dispatch_diag_method(method, params_json, id, host,
                                     &response)) {
      return response;
    }
  }
  {
    detail::RecordHandlers handlers;
    handlers.set_enabled = [this](bool on) { set_record_enabled(on); };
    handlers.poll_json = [this]() { return poll_record_events_json(); };
    handlers.clear = [this]() {
      std::lock_guard<std::mutex> lock(record_mu_);
      record_events_.clear();
    };
    if (detail::dispatch_record_method(method, params_json, id, handlers,
                                       &response)) {
      return response;
    }
  }
  return err_result(id, "unknown method: " + method);
}

void DebugAgent::confirm_dangerous_ops() {
  std::lock_guard<std::mutex> lock(policy_mu_);
  policy_.confirm_session();
}

bool DebugAgent::dangerous_ops_allowed() const {
  std::lock_guard<std::mutex> lock(policy_mu_);
  return policy_.is_dangerous_allowed();
}

std::string DebugAgent::exec_line(const std::string& line_in) {
  std::string line = line_in;
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.pop_back();
  }
  if (line.empty()) {
    return "";
  }
  if (line == ":help") {
    return detail::agent_help_text();
  }
  if (line == ":help json") {
    return detail::agent_methods_schema_json();
  }
  if (line == ":confirm") {
    confirm_dangerous_ops();
    return "dangerous ops confirmed for this session";
  }

  if (detail::AgentPolicy::is_dangerous_console_line(line) &&
      !dangerous_ops_allowed()) {
    detail::AgentPolicy::audit("deny", line);
    return "blocked: type :confirm once (or set SG_DEBUG_ALLOW=1; debug "
           "builds auto-allow)";
  }
  if (detail::AgentPolicy::is_dangerous_console_line(line)) {
    detail::AgentPolicy::audit("console", line);
  }

  std::string output;
  if (detail::exec_log_command(line, &output)) {
    return output;
  }
  {
    const DebugAgentHost host = copy_host();
    if (detail::exec_ask_command(line, host, this, &output)) {
      return output;
    }
    if (detail::exec_map_command(line, host, &output)) {
      return output;
    }
    if (detail::exec_ui_command(line, host, &output)) {
      return output;
    }
    if (detail::exec_script_command(line, host, &output)) {
      return output;
    }
    if (detail::exec_diag_command(line, host, &output)) {
      return output;
    }
  }
  {
    detail::RecordHandlers handlers;
    handlers.set_enabled = [this](bool on) { set_record_enabled(on); };
    handlers.poll_json = [this]() { return poll_record_events_json(); };
    handlers.clear = [this]() {
      std::lock_guard<std::mutex> lock(record_mu_);
      record_events_.clear();
    };
    if (detail::exec_record_command(line, handlers, &output)) {
      return output;
    }
  }
  if (detail::exec_harness_command(line, &output)) {
    return output;
  }
  if (detail::exec_sdbd_command(line, &output)) {
    return output;
  }
  {
    const detail::PyEvalFn eval = [this](const std::string& code) {
      return eval_python(code);
    };
    if (detail::exec_py_command(line, eval, &output)) {
      return output;
    }
  }
  if (line[0] == ':') {
    return "unknown command (see :help)";
  }
  return eval_python(line);
}

bool DebugAgent::spawn_python_worker() {
  return false;
}

DebugAgent& debug_agent() {
  static DebugAgent agent;
  return agent;
}

bool debug_console_env_enabled() {
  if (const char* env = base::switch_cstr("debug")) {
    if (env[0] == '1' && env[1] == '\0') {
      return true;
    }
  }
  return false;
}

void DebugAgent::set_record_enabled(bool on) {
  if (on) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    std::lock_guard<std::mutex> lock(record_mu_);
    record_t0_ms_ = static_cast<std::int64_t>(now);
    record_events_.clear();
    record_enabled_.store(true);
  } else {
    record_enabled_.store(false);
  }
}

void DebugAgent::push_record_event(const std::string& kind,
                                   const std::string& fields_json) {
  if (!record_enabled_.load()) {
    return;
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  std::int64_t t_ms = 0;
  {
    std::lock_guard<std::mutex> lock(record_mu_);
    t_ms = static_cast<std::int64_t>(now) - record_t0_ms_;
    if (t_ms < 0) {
      t_ms = 0;
    }
  }
  std::ostringstream oss;
  oss << "{\"src\":\"agent\",\"kind\":\"" << detail::json_escape(kind)
      << "\",\"t_ms\":" << t_ms;
  if (!fields_json.empty() && fields_json[0] == '{') {
    // Merge object fields: strip outer braces from fields_json.
    if (fields_json.size() >= 2) {
      oss << ',' << fields_json.substr(1, fields_json.size() - 2);
    }
  }
  oss << '}';
  std::lock_guard<std::mutex> lock(record_mu_);
  constexpr size_t kMax = 4096;
  if (record_events_.size() >= kMax) {
    record_events_.erase(record_events_.begin(),
                         record_events_.begin() + static_cast<std::ptrdiff_t>(kMax / 4));
  }
  record_events_.push_back(oss.str());
}

std::string DebugAgent::poll_record_events_json() {
  std::vector<std::string> batch;
  {
    std::lock_guard<std::mutex> lock(record_mu_);
    batch.swap(record_events_);
  }
  std::ostringstream oss;
  oss << '[';
  for (size_t i = 0; i < batch.size(); ++i) {
    if (i) {
      oss << ',';
    }
    oss << batch[i];
  }
  oss << ']';
  return oss.str();
}

void push_record_event(const std::string& kind, const std::string& fields_json) {
  debug_agent().push_record_event(kind, fields_json);
}

}  // namespace content
