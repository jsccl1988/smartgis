// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Starts every kind=python sample under plugin/runtime/samples/ plus the
// frozen python/samples/{hello,analysis,industry_pack,product_orchestrate}
// packs. Outside the frozen python/ tree. Mounts glang.ext.<id> before
// loading plugin.py so top-level `import glang.ext...` succeeds.

#include "base/core/log.h"
#include "content/browser/glang.runtime/exec/py/ext_module.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/manifest.h"
#include "plugin/runtime/extension/runtime.h"
#include "tool/command/command.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool read_text(const fs::path& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in || !out) {
    return false;
  }
  std::ostringstream oss;
  oss << in.rdbuf();
  *out = oss.str();
  return true;
}

bool start_pack(plugin::PythonRuntime* py,
                content::PluginHost* host,
                const fs::path& dir) {
  const fs::path json_path = dir / "plugin.json";
  const fs::path entry_path = dir / "plugin.py";
  if (!fs::exists(entry_path)) {
    std::fprintf(stderr, "missing plugin.py at %s\n", dir.string().c_str());
    return false;
  }

  std::string id;
  if (fs::exists(json_path)) {
    std::string body;
    plugin::Manifest m;
    std::string err;
    if (read_text(json_path, &body) && plugin::parse_manifest(body, &m, &err)) {
      id = m.id;
    }
  }
  if (id.empty()) {
    id = dir.filename().string();
  }

  const bool mounted = content::py::mount_extension(id, dir.string());
  if (!mounted) {
    LOGGING(LOG_WARNING, "sample_start_test: mount_extension soft-fail id=%s",
            id.c_str());
  }
  const bool ok = py->start(dir.string(), "plugin.py", host);
  py->stop(dir.string());
  content::py::unmount_extension(id);
  return ok;
}

}  // namespace

int main() {
  plugin::PythonRuntime py;
  if (!py.init()) {
#if defined(HAS_PYTHON)
    std::fprintf(stderr, "sample_start_test: python init failed\n");
    return 1;
#else
    std::puts("sample_start_test: ok");
    return 0;
#endif
  }

  tool::CommandCatalog catalog;
  content::EventBus bus;
  content::PluginHost* host =
      content::create_plugin_host(&catalog, &bus, nullptr);
  if (!host) {
    std::fprintf(stderr, "sample_start_test: no host\n");
    return 1;
  }
  py.bind_host(host);

  const std::vector<fs::path> packs = {
#ifdef PLUGIN_HELLO_EXT_DIR
      fs::path(PLUGIN_HELLO_EXT_DIR),
#endif
#ifdef PLUGIN_HELLO_DIR
      fs::path(PLUGIN_HELLO_DIR),
#endif
#ifdef PLUGIN_ANALYSIS_DIR
      fs::path(PLUGIN_ANALYSIS_DIR),
#endif
#ifdef PLUGIN_INDUSTRY_DIR
      fs::path(PLUGIN_INDUSTRY_DIR),
#endif
#ifdef PLUGIN_ORCHESTRATE_DIR
      fs::path(PLUGIN_ORCHESTRATE_DIR),
#endif
  };

  for (const fs::path& dir : packs) {
    if (dir.empty()) {
      continue;
    }
    const std::string label = dir.filename().string();
    expect(start_pack(&py, host, dir), (label + " start").c_str());
  }

  delete host;
  py.shutdown();

  if (g_fails) {
    std::fprintf(stderr, "sample_start_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("sample_start_test: ok");
  return 0;
}
