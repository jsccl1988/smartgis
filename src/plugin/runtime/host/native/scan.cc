// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/native/scan.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static_assert(offsetof(plugin::DiscoveredPlugin, directory) >
              offsetof(plugin::Manifest, startup));

namespace plugin {
namespace {

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

std::string first_dll(const std::filesystem::path& dir) {
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (ext == ".dll") {
      return entry.path().string();
    }
  }
  return {};
}

std::string join_dll(const std::filesystem::path& dir, const std::string& name) {
  if (name.empty()) {
    return {};
  }
  std::filesystem::path p(name);
  if (p.is_absolute()) {
    return p.string();
  }
  return (dir / p).string();
}

std::string folder_id(const std::filesystem::path& dir) {
  std::string stem = dir.filename().string();
  if (stem.empty()) {
    return {};
  }
  // plugin.json ids are dotted; folder-only packages use smartgis.<folder>.
  return std::string("smartgis.") + stem;
}

}  // namespace

std::vector<DiscoveredPlugin> scan_plugins_dir(const std::string& root,
                                               std::string* err) {
  std::vector<DiscoveredPlugin> out;
  if (err) {
    err->clear();
  }
  if (root.empty()) {
    if (err) {
      *err = "empty plugins dir";
    }
    return out;
  }
  std::error_code ec;
  const std::filesystem::path root_path(root);
  if (!std::filesystem::is_directory(root_path, ec)) {
    if (err) {
      *err = "plugins dir missing";
    }
    return out;
  }
  for (const auto& entry : std::filesystem::directory_iterator(root_path, ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }
    const std::filesystem::path dir = entry.path();
    DiscoveredPlugin d;
    d.directory = dir.string();
    const std::filesystem::path json_path = dir / "plugin.json";
    if (std::filesystem::is_regular_file(json_path, ec)) {
      const std::string body = read_file(json_path);
      std::string parse_err;
      if (!parse_manifest(body, &d.manifest, &parse_err)) {
        d.manifest.id = folder_id(dir);
        d.manifest.name = dir.filename().string();
        d.manifest.version = "0.0.0";
        d.manifest.kind = PluginKind::kNative;
        d.manifest.description = parse_err;
      }
    } else {
      d.manifest.id = folder_id(dir);
      d.manifest.name = dir.filename().string();
      d.manifest.version = "1.0.0";
      d.manifest.kind = PluginKind::kNative;
    }
    if (d.manifest.kind == PluginKind::kNative) {
      if (!d.manifest.library.empty()) {
        d.dll_path = join_dll(dir, d.manifest.library);
      } else {
        d.dll_path = first_dll(dir);
      }
    }
    if (d.manifest.id.empty()) {
      continue;
    }
    out.push_back(std::move(d));
  }
  return out;
}

namespace {

void copy_capped(char* dst, size_t cap, const std::string& src) {
  if (!dst || cap == 0) {
    return;
  }
  const size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
  if (n > 0) {
    std::memcpy(dst, src.data(), n);
  }
  dst[n] = '\0';
}

}  // namespace

void peek_plugins_dir_startup(const char* plugins_dir, PluginStartupPeek* out) {
  if (!out) {
    return;
  }
  out->scenario[0] = '\0';
  out->present[0] = '\0';
  out->fields[0] = '\0';
  if (!plugins_dir || !plugins_dir[0]) {
    return;
  }
  std::string err;
  const std::vector<DiscoveredPlugin> found =
      scan_plugins_dir(plugins_dir, &err);
  int best_scenario = 0;
  int best_present = 0;
  int best_fields = 0;
  bool have_scenario = false;
  bool have_present = false;
  bool have_fields = false;
  for (const DiscoveredPlugin& d : found) {
    const ManifestStartup& st = d.manifest.startup;
    if (!st.activate) {
      continue;
    }
    if (!st.scenario.empty() &&
        (!have_scenario || st.priority > best_scenario)) {
      have_scenario = true;
      best_scenario = st.priority;
      copy_capped(out->scenario, sizeof(out->scenario), st.scenario);
    }
    if (!st.present.empty() &&
        (!have_present || st.priority > best_present)) {
      have_present = true;
      best_present = st.priority;
      copy_capped(out->present, sizeof(out->present), st.present);
    }
    if (!st.fields.empty() && (!have_fields || st.priority > best_fields)) {
      have_fields = true;
      best_fields = st.priority;
      copy_capped(out->fields, sizeof(out->fields), st.fields);
    }
  }
}

}  // namespace plugin
