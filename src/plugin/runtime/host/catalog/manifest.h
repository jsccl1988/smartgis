// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MANIFEST_H_
#define PLUGIN_MANIFEST_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

enum class PluginKind { kBuiltin, kNative, kPython, kLegacyAm };

struct CommandContrib {
  std::string id;
  std::string title;
  std::string menu;
};

struct MenuContrib {
  std::string id;
  std::string title;
  std::string parent;
};

struct DockContrib {
  std::string id;
  std::string title;
  std::string area;
};

struct DialogContrib {
  std::string id;
  std::string title;
};

struct ProcessingContrib {
  std::string id;
  std::string title;
};

struct ManifestContributes {
  std::vector<CommandContrib> commands;
  std::vector<MenuContrib> menus;
  std::vector<DockContrib> docks;
  std::vector<DialogContrib> dialogs;
  std::vector<ProcessingContrib> processing;
};

// Product / harness launch block from plugin.json `startup`. Desktop shell
// applies this instead of Views CLI product selectors.
struct ManifestStartup {
  bool activate = true;
  std::string viewport;
  std::string seed;
  std::vector<std::string> commands;
  int priority = 0;
  // ScenarioRegistry id (e.g. plugin.flood). Empty = interactive product.
  std::string scenario;
  // present_dataset surface: main|preview. Empty = main.
  std::string present;
  // Optional atmosphere field ingest spec (world3d).
  std::string fields;
};

// Parsed plugin.json. Extra JSON keys are ignored.
struct Manifest {
  std::string id;
  std::string name;
  std::string version;
  int api_version = 0;
  PluginKind kind = PluginKind::kBuiltin;
  std::string author;
  std::string description;
  std::string homepage;
  std::string min_host_version = "1.0.0";
  std::string library;
  std::string entry;
  ManifestContributes contributes;
  ManifestStartup startup;
};

static_assert(offsetof(Manifest, startup) > offsetof(Manifest, contributes));

PLUGIN_HOST_EXPORT bool parse_manifest(std::string_view json,
                                       Manifest* out,
                                       std::string* err);

}  // namespace plugin

#endif  // PLUGIN_MANIFEST_H_
