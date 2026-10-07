// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/scenario_shell.h"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/processing/processing.h"

namespace plugin {
namespace detail {
namespace {

thread_local HarnessShell* g_plugin_shell = nullptr;
thread_local HarnessShell* g_atmosphere_shell = nullptr;

bool exe_dir_with_slash(wchar_t* path, size_t path_cch) {
  if (!path || path_cch == 0) {
    return false;
  }
  path[0] = L'\0';
  const DWORD n =
      GetModuleFileNameW(nullptr, path, static_cast<DWORD>(path_cch));
  if (n == 0 || n >= path_cch) {
    path[0] = L'\0';
    return false;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      return true;
    }
  }
  path[0] = L'\0';
  return false;
}

}  // namespace

void bind_plugin_scenario_shell(HarnessShell* shell) {
  g_plugin_shell = shell;
}

HarnessShell* plugin_scenario_shell() {
  return g_plugin_shell;
}

void plugin_mark(const char* step) {
  if (g_plugin_shell) {
    g_plugin_shell->mark_named(kMarkPlugin, step, false);
  }
}

void bind_atmosphere_scenario_shell(HarnessShell* shell) {
  g_atmosphere_shell = shell;
}

HarnessShell* atmosphere_scenario_shell() {
  return g_atmosphere_shell;
}

void atmosphere_mark(const char* step) {
  if (g_atmosphere_shell) {
    g_atmosphere_shell->mark_named(kMarkAtmosphere, step, false);
  }
}

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap) {
  if (!out_utf8 || out_cap < 2 || !rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!rels[i]) {
      continue;
    }
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, out_utf8,
                            static_cast<int>(out_cap), nullptr, nullptr) > 0) {
      return true;
    }
  }
  return false;
}

std::string json_escape_path(const char* path) {
  std::string out;
  if (!path) {
    return out;
  }
  for (const char* p = path; *p; ++p) {
    if (*p == '\\' || *p == '"') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
  return out;
}

bool run_processing_flushed(content::PluginHost* host, const char* id,
                            const std::string& args) {
  if (!host || !id || !*id || !host->run_processing(id, args)) {
    return false;
  }
  if (ProcessingPool* pool = processing_pool(host)) {
    pool->flush_for_test();
    return pool->last_ok();
  }
  return true;
}

}  // namespace detail
}  // namespace plugin
