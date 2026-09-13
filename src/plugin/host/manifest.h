// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MANIFEST_H_
#define PLUGIN_MANIFEST_H_

#include <string>
#include <string_view>
#include <vector>

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
};

bool parse_manifest(std::string_view json, Manifest* out, std::string* err);

}  // namespace plugin

#endif  // PLUGIN_MANIFEST_H_
