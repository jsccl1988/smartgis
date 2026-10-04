// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/cmd/agent_harness.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {
namespace {

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
    "ops_test.exe",
    "indexed_tin_test.exe",
    "proj_test.exe",
    "stat_expr_test.exe",
    "tin_delaunay_test.exe",
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
    "buffer_benchmark.exe",
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

}  // namespace

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

bool exec_harness_command(const std::string& line, std::string* output) {
  if (!output) {
    return false;
  }
  if (line == ":gis test") {
    *output = run_gis_harness(/*benches=*/false);
    return true;
  }
  if (line == ":gis bench") {
    *output = run_gis_harness(/*benches=*/true);
    return true;
  }
  if (line.rfind(":gis", 0) == 0) {
    *output = "usage: :gis test|bench";
    return true;
  }
  if (line == ":rhi test") {
    *output = run_rhi_harness(/*benches=*/false);
    return true;
  }
  if (line == ":rhi bench") {
    *output = run_rhi_harness(/*benches=*/true);
    return true;
  }
  if (line.rfind(":rhi", 0) == 0) {
    *output = "usage: :rhi test|bench";
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace content
