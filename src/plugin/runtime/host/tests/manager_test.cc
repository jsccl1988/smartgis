// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/native/manager.h"
#include "plugin/runtime/host/native/module.h"
#include "plugin/runtime/host/native/scan.h"
#include "plugin/runtime/host/catalog/registry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::filesystem::path make_pack(const std::filesystem::path& root,
                                const char* folder, const char* json) {
  const std::filesystem::path dir = root / folder;
  std::filesystem::create_directories(dir);
  std::ofstream out(dir / "plugin.json", std::ios::binary);
  out << json;
  return dir;
}

}  // namespace

int main() {
  plugin::NativeModule missing;
  expect(!missing.load("Z:\\no_such_plugin_dll.dll"), "load missing dll");

  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "smartgis_plugin_scan";
  std::error_code ec;
  std::filesystem::remove_all(tmp, ec);
  std::filesystem::create_directories(tmp);

  make_pack(tmp, "scanstub",
            "{\"id\":\"smartgis.scanstub\",\"name\":\"Scan Stub\","
            "\"version\":\"1.0.0\",\"api_version\":2,\"kind\":\"native\","
            "\"library\":\"scanstub.dll\","
            "\"startup\":{\"activate\":true,\"viewport\":\"map2d\","
            "\"priority\":7}}");
  make_pack(tmp, "map2d",
            "{\"id\":\"smartgis.map2d\",\"name\":\"Map2d\","
            "\"version\":\"1.0.0\",\"api_version\":2,\"kind\":\"builtin\"}");

  std::string err;
  const std::vector<plugin::DiscoveredPlugin> found =
      plugin::scan_plugins_dir(tmp.string(), &err);
  expect(found.size() >= 2, "scan two packages");
  bool saw_native = false;
  bool saw_builtin = false;
  for (const plugin::DiscoveredPlugin& d : found) {
    if (d.manifest.id == "smartgis.scanstub") {
      saw_native = d.manifest.kind == plugin::PluginKind::kNative;
      expect(!d.directory.empty(), "native dir");
      expect(d.manifest.startup.viewport == "map2d", "scan startup viewport");
      expect(d.manifest.startup.priority == 7, "scan startup priority");
    }
    if (d.manifest.id == "smartgis.map2d") {
      saw_builtin = d.manifest.kind == plugin::PluginKind::kBuiltin;
    }
  }
  expect(saw_native, "scanned native pack");
  expect(saw_builtin, "scanned builtin pack");

  plugin::Registry reg;
  plugin::PluginManager mgr(&reg);
  const int added = mgr.scan_directory(tmp.string());
  expect(added >= 2, "manager added packs");
  expect(reg.find("smartgis.scanstub") != nullptr, "registry has scanstub");
  expect(reg.find("smartgis.map2d") != nullptr, "registry has map2d");

  std::filesystem::remove_all(tmp, ec);
  if (g_fails) {
    std::fprintf(stderr, "plugin_manager_test %d FAIL(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "plugin_manager_test PASS\n");
  return 0;
}
