// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/common/plugin_io.h"

#include <string>

#include "app/views/il.runtime/backend/paths.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {
namespace {

thread_local HarnessShell* g_shell = nullptr;

}  // namespace

void bind_plugin_showcase_shell(HarnessShell* shell) {
  g_shell = shell;
}

HarnessShell* plugin_showcase_shell() {
  return g_shell;
}

void plugin_showcase_mark(const char* step) {
  if (g_shell) {
    g_shell->mark_named(kMarkPluginShowcase, step, false);
  }
}

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap) {
  return app::detail::resolve_first_existing_under_exe(rels, count, out_utf8,
                                                       out_cap);
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

}  // namespace detail
}  // namespace plugin
