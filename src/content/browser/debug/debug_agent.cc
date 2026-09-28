// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/debug_agent.h"

#include <atomic>
#include <chrono>
#include <cstdio>
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
#include "gis/datasource/provider/impl/sdbd/client/sdbd_client.h"

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

std::string json_escape(const std::string& s) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.String(s.c_str(), static_cast<rapidjson::SizeType>(s.size()));
  const char* p = buf.GetString();
  const size_t n = buf.GetSize();
  if (n >= 2 && p[0] == '"' && p[n - 1] == '"') {
    return std::string(p + 1, n - 2);
  }
  return std::string(p, n);
}

bool extract_string_field(const std::string& json,
                          const char* key,
                          std::string* out) {
  if (!out) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return true;
}

bool extract_int_field(const std::string& json, const char* key, int* out) {
  if (!out) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

std::string ok_result(int id, const std::string& result_obj) {
  std::ostringstream oss;
  oss << "{\"id\":" << id << ",\"ok\":true,\"result\":" << result_obj << "}";
  return oss.str();
}

std::string err_result(int id, const std::string& error) {
  std::ostringstream oss;
  oss << "{\"id\":" << id << ",\"ok\":false,\"error\":\"" << json_escape(error)
      << "\"}";
  return oss.str();
}

std::string ui_text_result(int id, const std::string& text) {
  return ok_result(id, std::string("{\"text\":\"") + json_escape(text) + "\"}");
}

std::string log_entry_json(const base::LogEntry& e) {
  std::ostringstream oss;
  oss << "{\"level\":\"" << base::log_level_name(e.level) << "\""
      << ",\"timestamp\":\"" << json_escape(e.timestamp) << "\""
      << ",\"tid\":" << e.tid << ",\"file\":\"" << json_escape(e.file) << "\""
      << ",\"line\":" << e.line << ",\"func\":\"" << json_escape(e.func) << "\""
      << ",\"message\":\"" << json_escape(e.message) << "\"}";
  return oss.str();
}

std::string discovery_path() {
  char buf[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, buf);
  if (n == 0 || n > MAX_PATH) {
    return "smartgis-debug.json";
  }
  return std::string(buf) + "smartgis-debug.json";
}

std::string run_python_code_oop(const std::string& code) {
  const char* py = std::getenv("SG_PYTHON");
  std::string exe = (py && py[0]) ? py : "python";
  const auto tmp = std::filesystem::temp_directory_path() /
                   ("sg_debug_py_" + std::to_string(GetCurrentProcessId()) +
                    ".py");
  {
    std::ofstream out(tmp, std::ios::binary);
    out << code;
  }
  std::string cmd = "\"" + exe + "\" \"" + tmp.string() + "\"";
  FILE* pipe = _popen(cmd.c_str(), "r");
  if (!pipe) {
    std::filesystem::remove(tmp);
    return std::string("error: failed to spawn python (") + exe + ")";
  }
  std::string output;
  char line[1024];
  while (std::fgets(line, sizeof(line), pipe)) {
    output += line;
  }
  const int rc = _pclose(pipe);
  std::filesystem::remove(tmp);
  if (rc != 0 && output.empty()) {
    return "error: python exited " + std::to_string(rc);
  }
  return output;
}

constexpr DWORD k_gis_child_timeout_ms = 60000;
constexpr size_t k_gis_output_cap = 4096;
constexpr size_t k_gis_fail_snip_cap = 512;

// Directory of the running process (typically out/Debug or out/Release).
std::filesystem::path process_exe_dir() {
  wchar_t buf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buf).parent_path();
}

// Prefer same dir as this process; fall back to sibling out/Debug.
std::filesystem::path resolve_harness_exe(const std::filesystem::path& dir,
                                          const char* name) {
  if (dir.empty() || !name || !name[0]) {
    return {};
  }
  std::error_code ec;
  const auto primary = dir / name;
  if (std::filesystem::is_regular_file(primary, ec)) {
    return primary;
  }
  const auto parent = dir.parent_path();
  const auto debug_sib = parent / "Debug" / name;
  if (std::filesystem::is_regular_file(debug_sib, ec)) {
    return debug_sib;
  }
  return {};
}

struct ChildRunResult {
  bool started = false;
  bool timed_out = false;
  DWORD exit_code = 1;
  std::string output;
};

// Spawn |exe| with no args; capture combined stdout/stderr; kill after timeout.
ChildRunResult run_child_exe(const std::filesystem::path& exe,
                             DWORD timeout_ms) {
  ChildRunResult result;
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
    return result;
  }
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = write_pipe;
  si.hStdError = write_pipe;

  // Quote path; tests take no arguments.
  std::wstring cmd = L"\"" + exe.wstring() + L"\"";
  std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back(L'\0');

  PROCESS_INFORMATION pi{};
  const BOOL ok =
      CreateProcessW(exe.wstring().c_str(), cmdline.data(), nullptr, nullptr,
                     TRUE, CREATE_NO_WINDOW, nullptr,
                     exe.parent_path().wstring().c_str(), &si, &pi);
  CloseHandle(write_pipe);
  write_pipe = nullptr;
  if (!ok) {
    CloseHandle(read_pipe);
    return result;
  }
  result.started = true;

  const auto deadline =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeout_ms);
  char buf[512];
  bool done = false;
  while (!done) {
    DWORD avail = 0;
    if (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr) &&
        avail > 0) {
      DWORD got = 0;
      const DWORD want =
          avail > sizeof(buf) ? static_cast<DWORD>(sizeof(buf)) : avail;
      if (ReadFile(read_pipe, buf, want, &got, nullptr) && got > 0) {
        if (result.output.size() < k_gis_output_cap) {
          const size_t room = k_gis_output_cap - result.output.size();
          result.output.append(buf, buf + (got < room ? got : room));
        }
      }
    }
    const DWORD wait = WaitForSingleObject(pi.hProcess, 50);
    if (wait == WAIT_OBJECT_0) {
      done = true;
      break;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      result.timed_out = true;
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 2000);
      done = true;
      break;
    }
  }

  // Drain remaining pipe bytes.
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(read_pipe, buf, sizeof(buf), &got, nullptr) || got == 0) {
      break;
    }
    if (result.output.size() < k_gis_output_cap) {
      const size_t room = k_gis_output_cap - result.output.size();
      result.output.append(buf, buf + (got < room ? got : room));
    }
  }

  if (!result.timed_out) {
    GetExitCodeProcess(pi.hProcess, &result.exit_code);
  } else {
    result.exit_code = 1;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(read_pipe);
  return result;
}

std::string first_failure_snip(const std::string& output) {
  if (output.empty()) {
    return {};
  }
  size_t end = 0;
  int lines = 0;
  while (end < output.size() && lines < 4) {
    const size_t nl = output.find('\n', end);
    if (nl == std::string::npos) {
      end = output.size();
      break;
    }
    end = nl + 1;
    ++lines;
  }
  std::string snip = output.substr(0, end);
  while (!snip.empty() && (snip.back() == '\n' || snip.back() == '\r')) {
    snip.pop_back();
  }
  if (snip.size() > k_gis_fail_snip_cap) {
    snip.resize(k_gis_fail_snip_cap);
  }
  return snip;
}

// Curated GIS unit-test stems (matrix / gis_test_all). Missing exes are skipped.
const char* const k_gis_test_exes[] = {
    "geo_ogr_test.exe",
    "proj_test.exe",
    "stat_expr_test.exe",
    "tin_delaunay_test.exe",
    "tin_xyz_test.exe",
    "datasource_session_test.exe",
    "sde_gdal_test.exe",
    "sdbd_client_test.exe",
    "ogr_text_encoding_test.exe",
    "feature_load_pipeline_test.exe",
    "feature_test.exe",
    "select_query_test.exe",
    "edit_conflict_test.exe",
    "style_test.exe",
    "tile_test.exe",
    "frame_test.exe",
    "world_test.exe",
    "dem_raster_test.exe",
    "land_mask_test.exe",
    "tessellate_style_test.exe",
    "model_test.exe",
    "tileset_test.exe",
    "field_store_test.exe",
    "procedural_test.exe",
    "field_ingest_test.exe",
    "cloud_system_test.exe",
    "ocean_system_test.exe",
    "environment_test.exe",
};

const char* const k_gis_bench_exes[] = {
    "geo_benchmark.exe",
    "proj_benchmark.exe",
    "datasource_benchmark.exe",
};

const char* const k_rhi_test_exes[] = {
    "rhi_suite_test.exe",
    "rhi_test.exe",
    "frame_graph_test.exe",
};

const char* const k_rhi_bench_exes[] = {
    "rhi_bench.exe",
};

std::string run_named_harness(const char* const* names, size_t count) {
  const auto dir = process_exe_dir();
  if (dir.empty()) {
    return "error: cannot resolve process directory";
  }

  int ok = 0;
  int fail = 0;
  int skipped = 0;
  std::string first_fail_name;
  std::string first_fail_snip;

  for (size_t i = 0; i < count; ++i) {
    const char* name = names[i];
    const auto path = resolve_harness_exe(dir, name);
    if (path.empty()) {
      ++skipped;
      continue;
    }
    const ChildRunResult r = run_child_exe(path, k_gis_child_timeout_ms);
    if (!r.started) {
      ++fail;
      if (first_fail_name.empty()) {
        first_fail_name = name;
        first_fail_snip = "failed to spawn";
      }
      continue;
    }
    if (r.timed_out) {
      ++fail;
      if (first_fail_name.empty()) {
        first_fail_name = name;
        first_fail_snip = "timeout >60s";
      }
      continue;
    }
    if (r.exit_code == 0) {
      ++ok;
    } else {
      ++fail;
      if (first_fail_name.empty()) {
        first_fail_name = name;
        first_fail_snip = first_failure_snip(r.output);
        if (first_fail_snip.empty()) {
          first_fail_snip = "exit " + std::to_string(r.exit_code);
        }
      }
    }
  }

  std::ostringstream out;
  out << "ok=" << ok << " fail=" << fail;
  if (skipped) {
    out << " skipped=" << skipped;
  }
  if (fail > 0 && !first_fail_name.empty()) {
    out << "\nfirst_fail=" << first_fail_name;
    if (!first_fail_snip.empty()) {
      out << "\n" << first_fail_snip;
    }
  }
  if (ok == 0 && fail == 0 && skipped > 0) {
    out << "\n(no harness exes found under " << dir.string() << ")";
  }
  return out.str();
}

std::string run_gis_harness(bool benches) {
  const char* const* names =
      benches ? k_gis_bench_exes : k_gis_test_exes;
  const size_t count =
      benches ? (sizeof(k_gis_bench_exes) / sizeof(k_gis_bench_exes[0]))
              : (sizeof(k_gis_test_exes) / sizeof(k_gis_test_exes[0]));
  return run_named_harness(names, count);
}

std::string run_rhi_harness(bool benches) {
  const char* const* names =
      benches ? k_rhi_bench_exes : k_rhi_test_exes;
  const size_t count =
      benches ? (sizeof(k_rhi_bench_exes) / sizeof(k_rhi_bench_exes[0]))
              : (sizeof(k_rhi_test_exes) / sizeof(k_rhi_test_exes[0]));
  return run_named_harness(names, count);
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

std::string DebugAgent::eval_python(const std::string& code) {
  DebugAgentHost host;
  {
    std::lock_guard<std::mutex> lock(mu_);
    host = host_;
  }
  if (host.py_eval) {
    return host.py_eval(code);
  }
  return run_python_code_oop(code);
}

bool DebugAgent::start() {
  if (running_.load()) {
    return true;
  }
  ensure_wsa();

  int want_port = 0;
  if (const char* env = std::getenv("SG_DEBUG_PORT")) {
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
      extract_string_field(line, "method", &method);
      if (method == "log.subscribe") {
        int id = 0;
        extract_int_field(line, "id", &id);
        if (sub_id) {
          base::log_sink().unsubscribe(sub_id);
        }
        sub_id = base::log_sink().subscribe([&](const base::LogEntry& e) {
          std::ostringstream oss;
          oss << "{\"event\":\"log\",\"entry\":" << log_entry_json(e) << "}\n";
          send_all(oss.str());
        });
        send_all(ok_result(id, "{}") + "\n");
        continue;
      }
      if (method == "py.register") {
        int id = 0;
        extract_int_field(line, "id", &id);
        {
          std::lock_guard<std::mutex> lock(py_mu_);
          if (py_sock_) {
            closesocket(static_cast<SOCKET>(py_sock_));
          }
          py_sock_ = sock_u;
        }
        send_all(ok_result(id, "{\"registered\":true}") + "\n");
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
    return err_result(id, "missing method");
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
  if (method == "ping") {
    return ok_result(id, "{\"pong\":true}");
  }
  if (method == "log.tail") {
    int n = 200;
    extract_int_field(params_json, "n", &n);
    if (n < 0) {
      n = 0;
    }
    const auto entries = base::log_sink().snapshot_tail(static_cast<size_t>(n));
    std::ostringstream oss;
    oss << "{\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
      if (i) {
        oss << ',';
      }
      oss << log_entry_json(entries[i]);
    }
    oss << "]}";
    return ok_result(id, oss.str());
  }
  if (method == "log.set_level") {
    std::string level_name;
    extract_string_field(params_json, "level", &level_name);
    base::LogLevel level = base::LogLevel::kInfo;
    if (!base::parse_log_level(level_name, &level)) {
      return err_result(id, "bad level");
    }
    base::log_sink().set_min_level(level);
    return ok_result(id, "{}");
  }
  if (method == "cmd.exec") {
    std::string line;
    extract_string_field(params_json, "line", &line);
    const std::string output = exec_line(line);
    return ok_result(id, std::string("{\"output\":\"") + json_escape(output) +
                             "\"}");
  }
  if (method == "sdbd.capabilities" || method == "sdbd.collections" ||
      method == "sdbd.query") {
    gis::datasource::SdbdClient client;
    gis::datasource::SdbdCallResult r;
    if (method == "sdbd.capabilities") {
      r = client.get_capabilities();
    } else if (method == "sdbd.collections") {
      r = client.get_collections();
    } else {
      std::string body;
      extract_string_field(params_json, "body", &body);
      r = client.post_query(body);
    }
    std::ostringstream oss;
    oss << "{\"ok\":" << (r.ok ? "true" : "false") << ",\"status\":" << r.status
        << ",\"body\":\"" << json_escape(r.body) << "\",\"error\":\""
        << json_escape(r.error) << "\"}";
    return ok_result(id, oss.str());
  }
  if (method == "py.eval") {
    std::string code;
    extract_string_field(params_json, "code", &code);
    const std::string out = eval_python(code);
    return ok_result(id, std::string("{\"output\":\"") + json_escape(out) +
                             "\"}");
  }
  if (method == "py.run_file") {
    std::string path;
    extract_string_field(params_json, "path", &path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      return err_result(id, "cannot read file");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string out = eval_python(ss.str());
    return ok_result(id, std::string("{\"output\":\"") + json_escape(out) +
                             "\"}");
  }
  if (method == "shutdown") {
    std::thread([this] { stop(); }).detach();
    return ok_result(id, "{}");
  }
  if (method == "ui.find") {
    std::string name;
    extract_string_field(params_json, "name", &name);
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_find) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_find(name));
  }
  if (method == "ui.click") {
    int x = 0;
    int y = 0;
    int button = 1;
    extract_int_field(params_json, "x", &x);
    extract_int_field(params_json, "y", &y);
    extract_int_field(params_json, "button", &button);
    if (button <= 0) {
      button = 1;
    }
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_click) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_click(x, y, button));
  }
  if (method == "ui.type") {
    std::string text;
    extract_string_field(params_json, "text", &text);
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_type) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_type(text));
  }
  if (method == "ui.dump_tree") {
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_dump_tree) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_dump_tree());
  }
  if (method == "ui.overlay_stats") {
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_overlay_stats) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_overlay_stats());
  }
  if (method == "ui.capture_shell") {
    std::string path;
    extract_string_field(params_json, "path", &path);
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.ui_capture_shell) {
      return err_result(id, "ui host not bound");
    }
    return ui_text_result(id, host.ui_capture_shell(path));
  }
  return err_result(id, "unknown method: " + method);
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
    return "commands: :help :clear :log.level <L> :refresh :extent :layers "
           ":ui find <name>|click <x> <y>|type <text>|tree|overlay|capture [path] "
           ":gis test|bench :rhi test|bench "
           ":sdbd capabilities|sql <text> :py <code> :run <path>";
  }
  if (line == ":clear") {
    base::log_sink().clear();
    return "cleared";
  }
  if (line.rfind(":log.level ", 0) == 0) {
    base::LogLevel level = base::LogLevel::kInfo;
    if (!base::parse_log_level(line.substr(11), &level)) {
      return "bad level";
    }
    base::log_sink().set_min_level(level);
    return std::string("min_level=") + base::log_level_name(level);
  }
  if (line == ":refresh") {
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (host.refresh_map) {
      host.refresh_map();
      return "refreshed";
    }
    return "no host.refresh_map";
  }
  if (line == ":extent") {
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (host.extent_string) {
      return host.extent_string();
    }
    return "no host.extent_string";
  }
  if (line == ":layers") {
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (!host.layer_names) {
      return "no host.layer_names";
    }
    std::ostringstream oss;
    const auto names = host.layer_names();
    for (size_t i = 0; i < names.size(); ++i) {
      if (i) {
        oss << '\n';
      }
      oss << names[i];
    }
    return oss.str();
  }
  if (line.rfind(":ui ", 0) == 0) {
    const std::string rest = line.substr(4);
    DebugAgentHost host;
    {
      std::lock_guard<std::mutex> lock(mu_);
      host = host_;
    }
    if (rest.rfind("find ", 0) == 0) {
      if (!host.ui_find) {
        return "ui host not bound";
      }
      return host.ui_find(rest.substr(5));
    }
    if (rest.rfind("click ", 0) == 0) {
      if (!host.ui_click) {
        return "ui host not bound";
      }
      int x = 0;
      int y = 0;
      std::istringstream iss(rest.substr(6));
      if (!(iss >> x >> y)) {
        return "usage: :ui click <x> <y>";
      }
      return host.ui_click(x, y, 1);
    }
    if (rest.rfind("type ", 0) == 0) {
      if (!host.ui_type) {
        return "ui host not bound";
      }
      return host.ui_type(rest.substr(5));
    }
    if (rest == "tree") {
      if (!host.ui_dump_tree) {
        return "ui host not bound";
      }
      return host.ui_dump_tree();
    }
    if (rest == "overlay") {
      if (!host.ui_overlay_stats) {
        return "ui host not bound";
      }
      return host.ui_overlay_stats();
    }
    if (rest == "capture" || rest.rfind("capture ", 0) == 0) {
      if (!host.ui_capture_shell) {
        return "ui host not bound";
      }
      std::string path;
      if (rest.size() > 8) {
        path = rest.substr(8);
      }
      return host.ui_capture_shell(path);
    }
    return "unknown :ui command (see :help)";
  }
  if (line == ":gis test") {
    return run_gis_harness(/*benches=*/false);
  }
  if (line == ":gis bench") {
    return run_gis_harness(/*benches=*/true);
  }
  if (line.rfind(":gis", 0) == 0) {
    return "usage: :gis test|bench";
  }
  if (line == ":rhi test") {
    return run_rhi_harness(/*benches=*/false);
  }
  if (line == ":rhi bench") {
    return run_rhi_harness(/*benches=*/true);
  }
  if (line.rfind(":rhi", 0) == 0) {
    return "usage: :rhi test|bench";
  }
  if (line == ":sdbd capabilities") {
    gis::datasource::SdbdClient client;
    auto r = client.get_capabilities();
    return r.ok ? r.body : ("error: " + r.error);
  }
  if (line.rfind(":sdbd sql ", 0) == 0) {
    const std::string sql = line.substr(10);
    gis::datasource::SdbdClient client;
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("sql");
    w.String(sql.c_str(), static_cast<rapidjson::SizeType>(sql.size()));
    w.EndObject();
    auto r = client.post_query(std::string(buf.GetString(), buf.GetSize()));
    return r.ok ? r.body : ("error: " + r.error);
  }
  if (line.rfind(":py ", 0) == 0) {
    return eval_python(line.substr(4));
  }
  if (line.rfind(":run ", 0) == 0) {
    std::ifstream in(line.substr(5), std::ios::binary);
    if (!in) {
      return "cannot read file";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    // Long :run stays OOP-capable via eval_python fallback when py_eval unset.
    return eval_python(ss.str());
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
  if (const char* env = std::getenv("SG_DEBUG")) {
    if (env[0] == '1' && env[1] == '\0') {
      return true;
    }
  }
  return false;
}

}  // namespace content
