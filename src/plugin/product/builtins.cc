// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/builtins.h"

#include <string>

#include "base/core/log.h"
#include "content/public/plugin_host.h"
#include "plugin/product/flood/commands.h"
#include "plugin/product/geochem/commands.h"
#include "plugin/product/mine/commands.h"
#include "plugin/product/print/commands.h"
#include "plugin/product/stormsurge/commands.h"
#include "plugin/product/traffic/commands.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/manifest/manifest.h"
#include "plugin/runtime/host/registry/registry.h"
#include "plugin/runtime/host/resources/resource_roots.h"
#include "plugin/runtime/processing/builtin_ops.h"

namespace plugin {
namespace {

void stop_noop() {}

bool add_builtin(Registry* registry,
                 const char* id,
                 const char* name,
                 bool (*start)(content::PluginHost*)) {
  Manifest m;
  m.id = id;
  m.name = name;
  m.version = "1.0.0";
  m.api_version = 2;
  m.kind = PluginKind::kBuiltin;
  if (!registry->add_manifest(m, TrustClass::kBuiltin)) {
    return false;
  }
  return registry->register_builtin_hooks(id, start, stop_noop);
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

void install_product_resource_roots(const std::string& plugins_dir) {
  if (plugins_dir.empty()) {
    return;
  }
  // Package folder names under --plugins-dir (not shared out/ui).
  set_resource_root("smartgis.world3d",
                    join_package(plugins_dir, "world3d"));
  set_resource_root("smartgis.traffic",
                    join_package(plugins_dir, "traffic"));
  set_resource_root("smartgis.flood", join_package(plugins_dir, "flood"));
  set_resource_root("smartgis.stormsurge",
                    join_package(plugins_dir, "stormsurge"));
  set_resource_root("smartgis.mine", join_package(plugins_dir, "mine"));
  set_resource_root("smartgis.geochem",
                    join_package(plugins_dir, "geochem"));
}

bool register_builtin_plugins(Registry* registry, content::PluginHost* host) {
  if (!registry || !host) {
    return false;
  }

  struct Builtin {
    const char* id;
    const char* name;
    bool (*start)(content::PluginHost*);
  };
  // Display names match leftover AuxModule Ambox labels (UTF-8).
  // Product packs only + shared processing catalog — chrome never lists these.
  const Builtin builtins[] = {
      {"smartgis.world3d", "DEM生成", register_world3d},
      {"smartgis.traffic", "城市交通最佳路径", register_traffic},
      {"smartgis.flood", "DEM洪水淹没", register_flood},
      {"smartgis.stormsurge", "风暴潮淹没", register_stormsurge},
      {"smartgis.mine", "矿山地层插值", register_mine},
      {"smartgis.geochem", "地球化学分析", register_geochem},
      {"smartgis.print", "地图打印", register_print},
      {"smartgis.processing", "Processing", register_builtin_processing},
  };

  for (const Builtin& b : builtins) {
    if (!add_builtin(registry, b.id, b.name, b.start)) {
      LOGGING(LOG_WARNING, "product builtins: add_builtin failed id=%s", b.id);
      continue;
    }
    if (!registry->set_enabled(b.id, true, host)) {
      LOGGING(LOG_WARNING, "product builtins: enable failed id=%s err=%s", b.id,
              registry->last_error().c_str());
      continue;
    }
  }
  return true;
}

}  // namespace plugin
