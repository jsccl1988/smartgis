// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/builtins.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "tool/command/command.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

int call_start_seh(bool (*start)(content::PluginHost*),
                   content::PluginHost* host) {
  if (!start) {
    return -1;
  }
  __try {
    return start(host) ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

}  // namespace

int main() {
  std::size_t n = 0;
  const app::BuiltinPlugin* rows = app::builtin_plugins(&n);
  expect(rows != nullptr, "builtin_plugins ptr");
  expect(n >= 1, "builtin_plugins count");
  if (!rows || n == 0) {
    return 1;
  }

  bool saw_processing = false;
  bool saw_map2d = false;
  for (std::size_t i = 0; i < n; ++i) {
    const app::BuiltinPlugin& b = rows[i];
    expect(b.id && b.id[0], "row id");
    expect(b.start != nullptr, b.id ? b.id : "null id");
    if (b.id && std::strcmp(b.id, "smartgis.processing") == 0) {
      saw_processing = true;
    }
    if (b.id && std::strcmp(b.id, "smartgis.map2d") == 0) {
      saw_map2d = true;
    }
  }
  expect(saw_processing, "table has processing");
  expect(!saw_map2d, "map2d is a native DLL, not append");

  tool::CommandCatalog catalog;
  content::EventBus events;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &events, nullptr);
  expect(host != nullptr, "create_plugin_host");
  if (!host) {
    return 1;
  }

  for (std::size_t i = 0; i < n; ++i) {
    const app::BuiltinPlugin& b = rows[i];
    if (!b.id || !b.start) {
      continue;
    }
    std::fprintf(stderr, "builtins_test: register %s\n", b.id);
    std::fflush(stderr);
    const int rc = call_start_seh(b.start, host);
    expect(rc >= 0, b.id);
    if (rc < 0) {
      std::fprintf(stderr, "FAIL: %s AV (call 0x0 / SEH)\n", b.id);
    }
  }

  delete host;
  host = content::create_plugin_host(&catalog, &events, nullptr);
  expect(host != nullptr, "create_plugin_host 2");
  if (!host) {
    return 1;
  }
  plugin::Registry reg;
  expect(app::register_builtin_plugins(&reg, host), "register_builtin_plugins");

  delete host;
  if (g_fails) {
    std::fprintf(stderr, "builtins_test %d FAIL(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "builtins_test PASS rows=%zu\n", n);
  return 0;
}
