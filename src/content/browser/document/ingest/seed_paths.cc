// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/ingest/seed_paths.h"

#include <filesystem>

namespace content {
namespace detail {

std::string path_stem(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  size_t begin = path.find_last_of("/\\");
  begin = (begin == std::string::npos) ? 0 : begin + 1;
  size_t end = path.find_last_of('.');
  if (end == std::string::npos || end < begin) {
    end = path.size();
  }
  return path.substr(begin, end - begin);
}

std::vector<std::string> style_seed_relative_paths() {
  // Intentionally empty: china_city.style.json disables carto remap. Hosts
  // that need an on-disk style must pass an explicit path; default china seed
  // uses in-memory default_carto_style_json via a null StyleDocument.
  return {};
}

}  // namespace detail

std::vector<std::string> china_seed_relative_paths() {
  // Prefer prefecture china_city (SmartGis.exe-like overview) over schematic
  // china_plp (~46 features). GN writes samples to out/data (exe is
  // out/Debug → ..\data). No synthetic / views_ogr_sample fallback — hard-fail
  // when real china packs are missing.
  return {
      "..\\data\\china_city.gpkg",
      "..\\data\\china_city.geojson",
      "china_city.gpkg",
      "china_city.geojson",
      "testing\\data\\china_city.gpkg",
      "testing\\data\\china_city.geojson",
      "..\\testing\\data\\china_city.gpkg",
      "..\\testing\\data\\china_city.geojson",
      "..\\..\\testing\\data\\china_city.gpkg",
      "..\\..\\testing\\data\\china_city.geojson",
      "china_plp.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_plp.geojson",
  };
}

bool try_resolve_china_seed_path(const std::string& exe_dir,
                                 std::string* out_path) {
  if (!out_path || exe_dir.empty()) {
    return false;
  }
  std::filesystem::path root(exe_dir);
  for (const std::string& rel : china_seed_relative_paths()) {
    std::filesystem::path cand = root / rel;
    std::error_code ec;
    if (std::filesystem::is_regular_file(cand, ec)) {
      *out_path = cand.lexically_normal().string();
      return true;
    }
  }
  return false;
}

}  // namespace content
