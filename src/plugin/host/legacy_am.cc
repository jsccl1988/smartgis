// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/host/legacy_am.h"

#include "plugin/host/manifest.h"
#include "plugin/host/registry.h"

#include <cctype>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#if defined(__has_include)
#if __has_include("pluginmanager.h")
#include "base/core/pluginmanager.h"
#define SMT_HAS_LEGACY_PLUGIN_MANAGER 1
#endif
#endif

namespace plugin {
namespace {

struct StemMap {
  const char* stem;
  const char* id;
};

constexpr StemMap kStems[] = {
    {"plugin_dem", "smartgis.dem"},
    {"plugin_proj", "smartgis.proj"},
    {"plugin_print", "smartgis.print"},
    {"plugin_model3d", "smartgis.model3d"},
    {"plugin_orthogrid", "smartgis.baogrid"},
    {"plugin_orthogrid", "smartgis.baogrid"},
};

struct NameMap {
  const char* name;
  const char* id;
};

constexpr NameMap kNames[] = {
    {"DEM生成", "smartgis.dem"},
    {"三维对象", "smartgis.model3d"},
    {"边界适应正交网格", "smartgis.baogrid"},
};

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

const char* mapped_stem(std::string_view stem) {
  for (const StemMap& row : kStems) {
    if (iequals(stem, row.stem)) {
      return row.id;
    }
  }
  return nullptr;
}

std::string lowercase_ascii(std::string_view in) {
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

std::string stem_from_filename(const char* name) {
  std::string s = name ? name : "";
  const size_t slash = s.find_last_of("\\/");
  if (slash != std::string::npos) {
    s = s.substr(slash + 1);
  }
  const size_t dot = s.find_last_of('.');
  if (dot != std::string::npos) {
    s = s.substr(0, dot);
  }
  return s;
}

}  // namespace

std::string legacy_id_from_stem_string(std::string_view stem) {
  if (const char* id = mapped_stem(stem)) {
    return id;
  }
  return std::string("legacy.") + lowercase_ascii(stem);
}

const char* legacy_id_from_stem(std::string_view stem) {
  if (const char* id = mapped_stem(stem)) {
    return id;
  }
  static thread_local std::string buf;
  buf = std::string("legacy.") + lowercase_ascii(stem);
  return buf.c_str();
}

const char* legacy_id_from_display_name(std::string_view name) {
  for (const NameMap& row : kNames) {
    if (name == row.name) {
      return row.id;
    }
  }
  return "";
}

bool scan_legacy_am(Registry* registry, const char* aux_module_dir) {
  if (!registry || !aux_module_dir || !aux_module_dir[0]) {
    return false;
  }

#if SMT_HAS_LEGACY_PLUGIN_MANAGER
  base::SmtPluginManager* mgr =
      base::SmtPluginManager::get_singleton_ptr();
  if (mgr) {
    mgr->load_all_plugin(aux_module_dir);
  }
#endif

  std::string pattern = aux_module_dir;
  if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/') {
    pattern += '\\';
  }
  pattern += "*.am";

  WIN32_FIND_DATAA fd{};
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return true;
  }
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    const std::string stem = stem_from_filename(fd.cFileName);
    Manifest m;
    m.id = legacy_id_from_stem_string(stem);
    m.name = stem;
    m.version = "1.0.0";
    m.api_version = 1;
    m.kind = PluginKind::kLegacyAm;
    m.library = fd.cFileName;
    registry->add_manifest(m, TrustClass::kBuiltin);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return true;
}

bool legacy_am_start(std::string_view id) {
  (void)id;
#if SMT_HAS_LEGACY_PLUGIN_MANAGER
  // Leftover LoadAllPlugin already StartPlugin's every *.am. Re-start is a
  // no-op for the adapter until a per-id lookup is added on leftover.
  return true;
#else
  return true;
#endif
}

void legacy_am_stop(std::string_view id) {
  (void)id;
}

void legacy_am_unload(std::string_view id) {
  (void)id;
}

}  // namespace plugin
