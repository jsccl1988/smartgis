// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/builtins.h"

#include <cstring>
#include <string>
#include <vector>

#include "base/core/log.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/manifest.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "plugin/runtime/host/catalog/resource_roots.h"

namespace app {
namespace {

void stop_noop() {}

std::vector<BuiltinPlugin>& appended_rows() {
  static std::vector<BuiltinPlugin> rows;
  return rows;
}

bool add_builtin(plugin::Registry* registry, const BuiltinPlugin& b) {
  if (!registry || !b.id || !b.start) {
    return false;
  }
  if (!registry->add_builtin_manifest(b.id, b.name ? b.name : b.id)) {
    return false;
  }
  return registry->register_builtin_hooks(b.id, b.start, stop_noop);
}

std::string join_package(const std::string& root, const char* package) {
  if (root.empty() || !package || !*package) {
    return {};
  }
  std::string out = root;
  const char last = out.back();
  if (last != '/' && last != '\\') {
    out.push_back('\\');
  }
  out.append(package);
  return out;
}

}  // namespace

bool append_builtin_plugin(const BuiltinPlugin& spec) {
  if (!spec.id || !spec.id[0] || !spec.start) {
    return false;
  }
  std::vector<BuiltinPlugin>& rows = appended_rows();
  for (const BuiltinPlugin& existing : rows) {
    if (existing.id && std::strcmp(existing.id, spec.id) == 0) {
      return false;
    }
  }
  rows.push_back(spec);
  return true;
}

const BuiltinPlugin* builtin_plugins(std::size_t* count) {
  const std::vector<BuiltinPlugin>& rows = appended_rows();
  if (count) {
    *count = rows.size();
  }
  return rows.empty() ? nullptr : rows.data();
}

void install_builtin_resource_roots(const std::string& plugins_dir) {
  if (plugins_dir.empty()) {
    return;
  }
  std::size_t n = 0;
  const BuiltinPlugin* rows = builtin_plugins(&n);
  if (!rows) {
    return;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const char* pkg = rows[i].resource_package;
    if (!rows[i].id || !pkg || !*pkg) {
      continue;
    }
    plugin::set_resource_root(rows[i].id, join_package(plugins_dir, pkg));
  }
}

bool register_builtin_plugins(plugin::Registry* registry,
                              content::PluginHost* host) {
  if (!registry || !host) {
    return false;
  }

  std::size_t n = 0;
  const BuiltinPlugin* rows = builtin_plugins(&n);
  if (!rows || n == 0) {
    return false;
  }

  bool any = false;
  for (std::size_t i = 0; i < n; ++i) {
    const BuiltinPlugin& b = rows[i];
    if (!b.id || !b.start) {
      LOGGING(LOG_ERROR, "plugin builtins: null registrar id=%s",
              b.id ? b.id : "(null)");
      continue;
    }
    if (!add_builtin(registry, b)) {
      LOGGING(LOG_WARNING, "plugin builtins: add_builtin failed id=%s", b.id);
      continue;
    }
    if (!registry->set_enabled(b.id, true, host)) {
      LOGGING(LOG_WARNING, "plugin builtins: enable failed id=%s err=%s", b.id,
              registry->last_error().c_str());
      continue;
    }
    any = true;
  }
  return any;
}

}  // namespace app
